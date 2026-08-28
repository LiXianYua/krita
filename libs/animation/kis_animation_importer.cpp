/*
 *  SPDX-FileCopyrightText: 2015 Jouni Pentikäinen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_animation_importer.h"
#include "kundo2magicstring.h"
#include <PkEventLoop.h>
#include <PkString.h>

#include "KoColorSpace.h"
#include <KoUpdater.h>
#include <deque>
#include <regex>
#include <string>
#include <vector>
#include "KisDocumentRegistry.h"
#include "KisDocument.h"
#include "kis_image.h"
#include "kis_undo_adapter.h"
#include "kis_paint_layer.h"
#include "kis_group_layer.h"
#include "kis_raster_keyframe_channel.h"
#include "kis_assign_profile_processing_visitor.h"
#include "commands/kis_image_layer_add_command.h"

namespace
{
class UndoMacroGuard
{
public:
    explicit UndoMacroGuard(KisUndoAdapter *undo)
        : m_undo(undo)
    {
    }

    ~UndoMacroGuard()
    {
        m_undo->endMacro();
    }

private:
    KisUndoAdapter *m_undo;
};

std::vector<std::string> filenameNumbers(const PkString &file, const std::regex &rx)
{
    const std::string utf8 = file.PkToUtf8();
    std::vector<std::string> numbers;
    for (std::sregex_iterator i(utf8.begin(), utf8.end(), rx), end; i != end; ++i) {
        numbers.push_back((*i)[1].str());
    }
    return numbers;
}

int numberToInt(const std::string &number, bool *ok)
{
    return PkString::PkFromUtf8(number.data(), static_cast<int>(number.size())).toInt(ok);
}
}

struct KisAnimationImporter::Private
{
    KisImageSP image;
    bool stop {false};
    bool trimFrames {false};
    KoUpdaterPtr updater;
};

KisAnimationImporter::KisAnimationImporter(KisImageSP image,
                                           KoUpdaterPtr updater,
                                           bool trimFrames)
    : m_d(new Private())
{
    m_d->image = image;
    m_d->trimFrames = trimFrames;
    m_d->updater = updater;
}

KisAnimationImporter::~KisAnimationImporter()
{}

KisImportExportErrorCode KisAnimationImporter::import(PkStringList files, int firstFrame, int step, bool autoAddHoldframes, bool startfrom0, int isAscending, bool assignDocumentProfile, PkList<int> optionalKeyframeTimeList)
{
    //TODO: We should clean up this code --
    // There are a lot of actions here that we should break into individual methods
    // so that we can better control code flow, and I'd prefer to use multiple import
    // calls to better handle all of these different options!
    // Additionally, we might prefer to use flags for multiple booleans to improve
    // legibility of calls.
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(step > 0, ImportExportCodes::InternalError);

    KisUndoAdapter *undo = m_d->image->undoAdapter();
    undo->beginMacro(kundo2_text("Import animation"));
    UndoMacroGuard undoMacroGuard(undo);

    PkScopedPointer<KisDocument> importDoc(KisDocumentRegistry::instance()->createDocument());
    importDoc->setFileBatchMode(true);

    const bool usingPredefinedTimes = !optionalKeyframeTimeList.isEmpty() && !autoAddHoldframes;
    std::deque<int> predefinedFrameQueue;
    for (int value : optionalKeyframeTimeList) {
        predefinedFrameQueue.push_back(value);
    }

    KisImportExportErrorCode status = ImportExportCodes::OK;
    int frame = usingPredefinedTimes ? predefinedFrameQueue.front() : firstFrame;
    if (usingPredefinedTimes) predefinedFrameQueue.pop_front();
    int filesProcessed = 0;

    if (usingPredefinedTimes) {
        KIS_ASSERT(files.count() == optionalKeyframeTimeList.count());
    }

    if (m_d->updater) {
        m_d->updater->setRange(0, files.size());
    }

    PkPair<KisPaintLayerSP, KisRasterKeyframeChannel*> layerRasterChannelPair;

    const std::regex rx("([0-9]+)");    // regex for extracting numbers
    std::vector<std::string> fileNumberRxList = filenameNumbers(files.at(0), rx);

    int firstFrameNumber = 0;
    bool ok = false;

    if (!fileNumberRxList.empty()) {
        numberToInt(fileNumberRxList.back(), &ok);    // selects the last number of file name of the first frame (useful for descending order)
        // Note to self -- ^^ uh.... This isn't doing anything?? Shouldn't this assign `firstFrameNumber`?
    }

    if (firstFrameNumber == 0){
        startfrom0 = false;     // if enabled, the zeroth frame will be places in -1 slot, leading to an error
    }

    const int offset = (startfrom0 ? 1 : 0);    //offset added to consider file numbering starts from 1 instead of 0
    int autoframe = 0;

    for (const PkString &file : files) {
        const bool successfullyLoaded = importDoc->openPath(file, KisDocument::DontAddToRecent);
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(successfullyLoaded, ImportExportCodes::InternalError);

        if ( (!usingPredefinedTimes && frame == firstFrame)
          || (usingPredefinedTimes && frame == optionalKeyframeTimeList.first()) ) {
             layerRasterChannelPair = initializePaintLayer(importDoc, undo);
        }

        filesProcessed++;

        if (m_d->updater) {
            if (m_d->updater->interrupted()) {
                m_d->stop = true;
            } else {
                m_d->updater->setValue(filesProcessed);

                // the updater doesn't call that automatically,
                // it is "threaded" by default
                PkEventLoop::processEvents();
            }
        }

        if (m_d->stop) {
            status = ImportExportCodes::Cancelled;
            break;
        }

        if (m_d->trimFrames) {
            importDoc->image()->projection()->crop(m_d->image->bounds());
        }
        importDoc->image()->projection()->purgeDefaultPixels();

        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(layerRasterChannelPair.second, ImportExportCodes::InternalError);

        if (!autoAddHoldframes) {
            layerRasterChannelPair.second->importFrame(frame, importDoc->image()->projection(), nullptr);    // as first frame added will go to second slot i.e #1 instead of #0
        } else {
            fileNumberRxList = filenameNumbers(file, rx);
            ok = false;
            const int filenum = fileNumberRxList.empty() ? 0 : numberToInt(fileNumberRxList.back(), &ok);

            if (isAscending == 0) {
                autoframe = firstFrame + filenum - offset;
            } else {
                autoframe = firstFrame + (firstFrameNumber - filenum); //places the first frame #0 (or #1) slot, and later frames are added as per the difference
            }

            if (ok) {
                layerRasterChannelPair.second->importFrame(autoframe , importDoc->image()->projection(), nullptr);
            } else {
                // if it fails to extract a number, the next frame will simply be added to next slot
                layerRasterChannelPair.second->importFrame(autoframe + 1, importDoc->image()->projection(), nullptr);
            }
        }

        if (usingPredefinedTimes && !predefinedFrameQueue.empty()) {
            frame = predefinedFrameQueue.front();
            predefinedFrameQueue.pop_front();
        } else {
            frame += step;
        }
    }

    if (layerRasterChannelPair.first && assignDocumentProfile) {

        if (layerRasterChannelPair.first->colorSpace()->colorModelId() == m_d->image->colorSpace()->colorModelId()) {

            const KoColorSpace *srcColorSpace = layerRasterChannelPair.first->colorSpace();
            const KoColorSpace *dstColorSpace = KoColorSpaceRegistry::instance()->colorSpace(
                        srcColorSpace->colorModelId().id()
                        , srcColorSpace->colorDepthId().id()
                        , m_d->image->colorSpace()->profile());

            KisAssignProfileProcessingVisitor *visitor = new KisAssignProfileProcessingVisitor(srcColorSpace, dstColorSpace);
            visitor->visit(layerRasterChannelPair.first.data(), undo);
        }
    }

    return status;
}

PkPair<KisPaintLayerSP, KisRasterKeyframeChannel*> KisAnimationImporter::initializePaintLayer(PkScopedPointer<KisDocument>& doc, KisUndoAdapter *undoAdapter)
{
    const KoColorSpace *cs = doc->image()->colorSpace();
    KisPaintLayerSP paintLayer = new KisPaintLayer(m_d->image, m_d->image->nextLayerName(), OPACITY_OPAQUE_U8, cs);
    undoAdapter->addCommand(new KisImageLayerAddCommand(m_d->image, paintLayer, m_d->image->rootLayer(), m_d->image->rootLayer()->childCount()));

    paintLayer->enableAnimation();
    KisRasterKeyframeChannel* contentChannel = dynamic_cast<KisRasterKeyframeChannel*>(paintLayer->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true));
    return PkPair<KisPaintLayerSP, KisRasterKeyframeChannel*>(paintLayer, contentChannel);
}

void KisAnimationImporter::cancel()
{
    m_d->stop = true;
}
