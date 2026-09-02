/*
 *  SPDX-FileCopyrightText: 2016 Laszlo Fazekas <mneko@freemail.hu>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisDocument.h>

#include "csv_saver.h"

#include <filesystem>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <system_error>

#include <PkFileStream.h>
#include <PkScopedPointer.h>
#include <PkStream.h>
#include <PkRect.h>
#include <PkVector.h>

#include <KisDocumentRegistry.h>
#include <KisMimeDatabase.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <kis_annotation.h>
#include <kis_debug.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_image_animation_interface.h>
#include <kis_iterator_ng.h>
#include <kis_paint_device.h>
#include <kis_paint_layer.h>
#include <KisPngCodec.h>
#include <kis_raster_keyframe_channel.h>
#include <kis_time_span.h>
#include <kis_types.h>

#include "csv_layer_record.h"
#include "csv_write_all.h"

namespace
{
using CsvPrivate::writeAll;

std::string fixedSix(double value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

std::string paddedFive(int value)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setw(5) << std::setfill('0') << value;
    return stream.str();
}

PkString sanitizedLayerName(const PkString &name)
{
    std::string bytes = name.PkToUtf8();
    for (char &byte : bytes) {
        if (byte == '"' || byte == '\r' || byte == '\n') {
            byte = '_';
        }
    }
    return PkString::PkFromUtf8(bytes.data(), static_cast<int>(bytes.size()));
}
}

CSVSaver::CSVSaver(KisDocument *doc, bool batchMode)
    : m_image(doc->savingImage())
    , m_doc(doc)
    , m_stop(false)
{
    (void)batchMode;
}

CSVSaver::~CSVSaver()
{
}

KisImageSP CSVSaver::image()
{
    return m_image;
}

KisImportExportErrorCode CSVSaver::encode(PkStream *io)
{
    int idx;
    int start, end;
    KisNodeSP node;
//    KisTimeKeyframePair keyframeEntry;
    KisKeyframeSP keyframe;
    PkVector<CSVLayerRecord*> layers;

    KisImageAnimationInterface *animation = m_image->animationInterface();

    //Using the original local path
    PkString path = m_doc->localFilePath();

    if (path.right(4).toUpper() == ".CSV")
        path = path.left(path.size() - 4);
    else {
        // something is wrong: the local file name is not .csv!
        // trying the given (probably temporary) filename as well

        KIS_SAFE_ASSERT_RECOVER(0 && "Wrong extension of the saved file!") {
            path = path.left(path.size() - 4);
        }
    }
    path.append(".frames");

    std::error_code directoryError;
    const std::filesystem::path framesDirectory = std::filesystem::u8path(path.PkToUtf8());
    std::filesystem::create_directories(framesDirectory, directoryError);
    if (directoryError || !std::filesystem::is_directory(framesDirectory, directoryError)) {
        return ImportExportCodes::NoAccessToWrite;
    }
    path.append("/");

    node = m_image->rootLayer()->firstChild();

    //TODO: correct handling of the layer tree.
    //for now, only top level paint layers are saved

    idx = 0;

    while (node) {
        if (KisLayer *paintLayer = dynamic_cast<KisLayer *>(node.data())) {
            CSVLayerRecord* layerRecord = new CSVLayerRecord();
            layers.prepend(layerRecord); //reverse order!

            layerRecord->name = sanitizedLayerName(paintLayer->name());

            if (layerRecord->name.isEmpty())
                layerRecord->name= PkString("Unnamed-%1").arg(idx);

            layerRecord->visible = (paintLayer->visible()) ? 1 : 0;
            layerRecord->density = (float)(paintLayer->opacity()) / OPACITY_OPAQUE_U8;
            layerRecord->blending = convertToBlending(paintLayer->compositeOpId());
            layerRecord->layer = paintLayer;
            layerRecord->channel = paintLayer->original()->keyframeChannel();
            layerRecord->last = "";
            layerRecord->frame = 0;
            idx++;
        }
        node = node->nextSibling();
    }

    KisTimeSpan range = animation->documentPlaybackRange();

    start = (range.isValid()) ? range.start() : 0;

    if (!range.isInfinite()) {
        end = range.end();

        if (end < start) end = start;
    } else {
        //undefined length, searching for the last keyframe
        end = start;
        int keyframeTime;

        for (idx = 0; idx < layers.size(); idx++) {
            KisRasterKeyframeChannel *channel = layers.at(idx)->channel;

            if (channel) {
                keyframeTime = channel->lastKeyframeTime();

                if ( (channel->keyframeAt(keyframeTime)) && (keyframeTime > end) )
                    end = keyframeTime;
            }
        }
    }

    //create temporary doc for exporting
    PkScopedPointer<KisDocument> exportDoc(KisDocumentRegistry::instance()->createDocument());
    createTempImage(exportDoc.data());

    KisImportExportErrorCode retval= ImportExportCodes::OK;

    int frame = start;
    int step = 0;

    do {
        // Cancellation is cooperative at record boundaries; the headless core
        // has no UI event queue to pump.
        if (m_stop.load()) {
            retval = ImportExportCodes::Cancelled;
            break;
        }

        switch(step) {
        case 0:
            if (!CsvPrivate::writeAll(io, "UTF-8, TVPaint, \"CSV 1.0\"\r\n")) retval = ImportExportCodes::Failure;
            break;
        case 1:
            if (!writeAll(io, "Project Name, Width, Height, Frame Count, Layer Count, Frame Rate, Pixel Aspect Ratio, Field Mode\r\n")) {
                retval = ImportExportCodes::Failure;
            }
            break;
        case 2:
            if (!writeAll(io, "\"" + m_image->objectName().PkToUtf8() + "\", ")
                || !writeAll(io, std::to_string(m_image->width()) + ", " + std::to_string(m_image->height()) + ", ")
                || !writeAll(io, std::to_string(end - start + 1) + ", " + std::to_string(layers.size()) + ", ")
                || !writeAll(io, fixedSix(static_cast<double>(animation->framerate())) + ", ")
                || !writeAll(io, fixedSix(m_image->xRes() / m_image->yRes()) + ", Progressive\r\n")) {
                retval = ImportExportCodes::Failure;
            }
            break;
        case 3:
            if (!writeAll(io, "#Layers")) {
                retval = ImportExportCodes::Failure;
                break;
            }
            for (idx = 0; idx < layers.size(); idx++) {
                if (!writeAll(io, ", \"" + layers.at(idx)->name.PkToUtf8() + "\"")) break;
            }
            if (idx < layers.size()) retval = ImportExportCodes::Failure;
            break;
        case 4:
            if (!writeAll(io, "\r\n#Density")) {
                retval = ImportExportCodes::Failure;
                break;
            }
            for (idx = 0; idx < layers.size(); idx++) {
                if (!writeAll(io, ", " + fixedSix(static_cast<double>(layers.at(idx)->density)))) break;
            }
            if (idx < layers.size()) retval = ImportExportCodes::Failure;
            break;
        case 5:
            if (!writeAll(io, "\r\n#Blending")) {
                retval = ImportExportCodes::Failure;
                break;
            }
            for (idx = 0; idx < layers.size(); idx++) {
                if (!writeAll(io, ", \"" + layers.at(idx)->blending.PkToUtf8() + "\"")) break;
            }
            if (idx < layers.size()) retval = ImportExportCodes::Failure;
            break;
        case 6:
            if (!writeAll(io, "\r\n#Visible")) {
                retval = ImportExportCodes::Failure;
                break;
            }
            for (idx = 0; idx < layers.size(); idx++) {
                if (!writeAll(io, ", " + std::to_string(layers.at(idx)->visible))) break;
            }
            if (idx < layers.size()) retval = ImportExportCodes::Failure;
            break;
        default:
            if (frame > end) {
                if (!writeAll(io, "\r\n")) retval = ImportExportCodes::Failure;
                step = 8;
                break;
            }

            if (!writeAll(io, "\r\n#" + paddedFive(frame))) {
                retval = ImportExportCodes::Failure;
                break;
            }

            for (idx = 0; idx < layers.size(); idx++) {
                CSVLayerRecord *layer = layers.at(idx);
                KisRasterKeyframeChannel *channel = layer->channel;
                if (channel) {
                    keyframe = frame == start ? channel->activeKeyframeAt(frame) : channel->keyframeAt(frame);
                } else {
                    keyframe.clear();
                }

                if (keyframe || frame == start) {
                    retval = getLayer(layer, exportDoc.data(), keyframe, path, frame, idx);
                    if (!retval.isOk()) break;
                }
                if (!writeAll(io, ", \"" + layer->last.PkToUtf8() + "\"")) break;
            }
            if (idx < layers.size()) retval = ImportExportCodes::Failure;

            frame++;
            step = 6;
            break;
        }
        step++;
    } while (retval.isOk() && step < 8);

    for (CSVLayerRecord *layer : layers) {
        delete layer;
    }

    return retval;
}

PkString CSVSaver::convertToBlending(const PkString &opid)
{
    if (opid == COMPOSITE_OVER) return "Color";
    if (opid == COMPOSITE_BEHIND) return "Behind";
    if (opid == COMPOSITE_ERASE) return "Erase";
    // "Shade"
    if (opid == COMPOSITE_LINEAR_LIGHT) return "Light";
    if (opid == COMPOSITE_COLORIZE) return "Colorize";
    if (opid == COMPOSITE_HUE) return "Hue";
    if ((opid == COMPOSITE_ADD) ||
        (opid == COMPOSITE_LINEAR_DODGE)) return "Add";
    if (opid == COMPOSITE_INVERSE_SUBTRACT) return "Sub";
    if (opid == COMPOSITE_MULT) return "Multiply";
    if (opid == COMPOSITE_SCREEN) return "Screen";
    // "Replace"
    // "Substitute"
    if (opid == COMPOSITE_DIFF) return "Difference";
    if (opid == COMPOSITE_DIVIDE) return "Divide";
    if (opid == COMPOSITE_OVERLAY) return "Overlay";
    if (opid == COMPOSITE_DODGE) return "Light2";
    if (opid == COMPOSITE_BURN) return "Shade2";
    if (opid == COMPOSITE_HARD_LIGHT) return "HardLight";
    if ((opid == COMPOSITE_SOFT_LIGHT_PHOTOSHOP) ||
        (opid == COMPOSITE_SOFT_LIGHT_SVG)) return "SoftLight";
    if (opid == COMPOSITE_GRAIN_EXTRACT) return "GrainExtract";
    if (opid == COMPOSITE_GRAIN_MERGE) return "GrainMerge";
    if (opid == COMPOSITE_SUBTRACT) return "Sub2";
    if (opid == COMPOSITE_DARKEN) return "Darken";
    if (opid == COMPOSITE_LIGHTEN) return "Lighten";
    if (opid == COMPOSITE_SATURATION) return "Saturation";

    return "Color";
}

KisImportExportErrorCode CSVSaver::getLayer(CSVLayerRecord* layer, KisDocument* exportDoc, KisKeyframeSP keyframe, const PkString &path, int frame, int idx)
{
    //render to the temp layer
    KisImageSP image = exportDoc->savingImage();

    if (!image) image= exportDoc->image();

    KisPaintDeviceSP device = image->rootLayer()->firstChild()->projection();

    if (!keyframe.isNull()) {
        KisRasterKeyframeSP rasterKeyframe = keyframe.dynamicCast<KisRasterKeyframe>();
        if (rasterKeyframe) {
            rasterKeyframe->writeFrameToDevice(device);
        }
    } else {
        device->makeCloneFrom(layer->layer->projection(),image->bounds()); // without animation
    }
    PkRect bounds = device->exactBounds();

    if (bounds.isEmpty()) {
        layer->last = "";                   //empty frame
        return ImportExportCodes::OK;
    }
    const std::string frameName = "frame" + paddedFive(idx + 1) + "-" + paddedFive(frame) + ".png";
    layer->last = PkString::PkFromUtf8(frameName.data(), static_cast<int>(frameName.size()));

    PkString filename = path;
    filename.append(layer->last);

    //save to PNG
    KisSequentialConstIterator it(device, image->bounds());
    const KoColorSpace* cs = device->colorSpace();

    bool isThereAlpha = false;
    while (it.nextPixel()) {
        if (cs->opacityU8(it.oldRawData()) != OPACITY_OPAQUE_U8) {
            isThereAlpha = true;
            break;
        }
    }

    if (!KisPngCodec::isColorSpaceSupported(cs)) {
        device = new KisPaintDevice(*device.data());
        device->convertTo(KoColorSpaceRegistry::instance()->rgb8());
    }
    KisPNGOptions options;

    options.alpha = isThereAlpha;
    options.interlace = false;
    options.compression = 8;
    options.tryToSaveAsIndexed = false;
    options.transparencyFillColor = PkColor(0,0,0);
    options.saveSRGBProfile = true;                 //TVPaint can use only sRGB
    options.forceSRGB = false;

    PkFileStream frameFile(filename);
    if (!frameFile.open(PkStream::WriteOnly | PkStream::Truncate)) {
        return ImportExportCodes::NoAccessToWrite;
    }

    KisPngCodec kpc;

    KisImportExportErrorCode result = kpc.buildFile(&frameFile, image->bounds(),
                                                    image->xRes(), image->yRes(), device,
                                                    image->beginAnnotations(), image->endAnnotations(),
                                                    options, (KisMetaData::Store* )0 );

    return result;
}

void CSVSaver::createTempImage(KisDocument* exportDoc)
{
    exportDoc->setInfiniteAutoSaveInterval();
    exportDoc->setFileBatchMode(true);

    KisImageSP exportImage = new KisImage(exportDoc->createUndoStore(),
                                           m_image->width(), m_image->height(), m_image->colorSpace(),
                                           PkString());

    exportImage->setResolution(m_image->xRes(), m_image->yRes());
    exportDoc->setCurrentImage(exportImage);

    KisPaintLayer* paintLayer = new KisPaintLayer(exportImage, "paint device", OPACITY_OPAQUE_U8);
    exportImage->addNode(paintLayer, exportImage->rootLayer(), KisLayerSP(0));
}


KisImportExportErrorCode CSVSaver::buildAnimation(PkStream *io)
{
    KIS_ASSERT_RECOVER_RETURN_VALUE(m_image, ImportExportCodes::InternalError);
    return encode(io);
}

void CSVSaver::cancel()
{
    m_stop = true;
}
