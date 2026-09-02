/*
 *  SPDX-FileCopyrightText: 2016 Laszlo Fazekas <mneko@freemail.hu>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisDocument.h>

#include "csv_loader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>

#include <PkScopedPointer.h>
#include <PkVector.h>
#include <PkStream.h>

#include <KisDocumentRegistry.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KoCompositeOpRegistry.h>

#include <kis_debug.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_raster_keyframe_channel.h>
#include <kis_image_animation_interface.h>
#include <kis_time_span.h>

#include "csv_read_line.h"
#include "csv_layer_record.h"

namespace
{
PkString pathWithSlash(const std::filesystem::path &path)
{
    std::string value = path.generic_u8string();
    if (!value.empty() && value.back() != '/') {
        value.push_back('/');
    }
    return PkString::PkFromUtf8(value.data(), static_cast<int>(value.size()));
}

bool isDirectory(const std::filesystem::path &path)
{
    std::error_code error;
    return std::filesystem::is_directory(path, error) && !error;
}

PkString defaultFramesPath(const PkString &filename)
{
    std::string value = filename.PkToUtf8();
    if (value.size() >= 4) {
        std::string extension = value.substr(value.size() - 4);
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        if (extension == ".CSV") {
            value.resize(value.size() - 4);
        }
    }
    value += ".frames/";
    return PkString::PkFromUtf8(value.data(), static_cast<int>(value.size()));
}
}

CSVLoader::CSVLoader(KisDocument *doc, bool batchMode)
    : m_image(0)
    , m_doc(doc)
    , m_stop(false)
{
    (void)batchMode;
}

CSVLoader::~CSVLoader()
{
}

KisImportExportErrorCode CSVLoader::decode(PkStream *io, const PkString &filename)
{
    PkString     field;
    int         idx = 0;
    int         frame = 0;

    PkString     projName;
    int         width = 0;
    int         height = 0;
    int         frameCount = 1;
    float       framerate = 24.0;
    float       pixelRatio = 1.0;

    int         projNameIdx = -1;
    int         widthIdx = -1;
    int         heightIdx = -1;
    int         frameCountIdx = -1;
    int         framerateIdx = -1;
    int         pixelRatioIdx = -1;

    PkVector<CSVLayerRecord*> layers;

    const std::filesystem::path sourcePath = std::filesystem::u8path(filename.PkToUtf8());
    const PkString base = pathWithSlash(sourcePath.parent_path());
    const PkString path = defaultFramesPath(filename);

    KisImportExportErrorCode retval = ImportExportCodes::OK;

    dbgFile << "pos:" << io->pos();

    CSVReadLine readLine;
    PkScopedPointer<KisDocument> importDoc(KisDocumentRegistry::instance()->createDocument());
    importDoc->setInfiniteAutoSaveInterval();
    importDoc->setFileBatchMode(true);

    int step = 0;

    do {
        // Cancellation is cooperative at record boundaries. No UI event pump is
        // required; callers may invoke cancel() from their orchestration layer.
        if (m_stop.load()) {
            retval = ImportExportCodes::Cancelled;
            break;
        }

        if ((idx = readLine.nextLine(io)) <= 0) {
            if ((idx < 0) ||(step < 5))
                retval = ImportExportCodes::FileFormatIncorrect;
            break;
        }
        if (!readLine.nextField(&field)) continue;

        switch (step) {

        case 0 :    //skip first row
            step = 1;
            break;

        case 1 :    //scene header names
            step = 2;

            for (idx = 0;; idx++) {
                if (field == "Project Name") {
                    projNameIdx = idx;

                } else if (field == "Width") {
                    widthIdx = idx;

                } else if (field == "Height") {
                    heightIdx = idx;

                } else if (field == "Frame Count") {
                    frameCountIdx = idx;

                } else if (field == "Frame Rate") {
                    framerateIdx = idx;

                } else if (field == "Pixel Aspect Ratio") {
                    pixelRatioIdx = idx;
                }
                if (!readLine.nextField(&field)) break;
            }
            break;

        case 2 : {  //scene header values
            step= 3;

            bool hasProjectName = false;
            for (idx= 0;; idx++) {
                if (idx == projNameIdx) {
                    projName = field;
                    hasProjectName = true;

                } else if (idx == widthIdx) {
                    width = field.toInt();

                } else if (idx == heightIdx) {
                    height = field.toInt();

                } else if (idx == frameCountIdx) {
                    frameCount = field.toInt();

                    if (frameCount < 1) frameCount= 1;

                } else if (idx == framerateIdx) {
                    framerate = static_cast<float>(field.toDouble());

                } else if (idx == pixelRatioIdx) {
                    pixelRatio = static_cast<float>(field.toDouble());

                }
                if (!readLine.nextField(&field)) break;
            }

            if ((width < 1) || (height < 1)) {
               retval = ImportExportCodes::Failure;
               break;
            }

            retval = createNewImage(width, height, pixelRatio, hasProjectName ? projName : filename);
            break;
        }

        case 3 :    //create level headers
            if (field[0] != '#') break;

            while (readLine.nextField(&field)) {
                CSVLayerRecord* layerRecord = new CSVLayerRecord();
                layers.append(layerRecord);
            }
            readLine.rewind();
            if (!readLine.nextField(&field)) break;
            step = 4;
            [[fallthrough]];

        case 4 :    //level header

            if (field == "#Layers") {
                //layer name
                for (idx = 0; idx < layers.size() && readLine.nextField(&field); idx++)
                    layers.at(idx)->name = field;

                break;
            }
            if (field == "#Density") {
                //layer opacity
                for (idx = 0; idx < layers.size() && readLine.nextField(&field); idx++)
                    layers.at(idx)->density = static_cast<float>(field.toDouble());

                break;
            }
            if (field == "#Blending") {
                //layer blending mode
                for (idx = 0; idx < layers.size() && readLine.nextField(&field); idx++)
                    layers.at(idx)->blending = field;

                break;
            }
            if (field == "#Visible") {
                //layer visibility
                for (idx = 0; idx < layers.size() && readLine.nextField(&field); idx++)
                    layers.at(idx)->visible = field.toInt();

                break;
            }
            if (field == "#Folder") {
                //CSV 1.1 folder location
                for (idx = 0; idx < layers.size() && readLine.nextField(&field); idx++) {
                    CSVLayerRecord *layer = layers.at(idx);
                    layer->path = validPath(field, base);
                    layer->hasPath = !layer->path.isEmpty();
                }

                break;
            }
            if ((field.size() < 2) || (field[0] != u'#') || field[1] < u'0' || field[1] > u'9') break;

            step = 5;

            [[fallthrough]];

        case 5 :    //frames

            if ((field.size() < 2) || (field[0] != u'#') || field[1] < u'0' || field[1] > u'9') break;

            for (idx = 0; idx < layers.size() && readLine.nextField(&field); idx++) {
                CSVLayerRecord* layer = layers.at(idx);

                if (layer->last != field) {
                    retval = setLayer(layer, importDoc.data(), path);
                    layer->last = field;
                    layer->frame = frame;
                }
            }
            frame++;
            break;
        }
    } while (retval.isOk());

    //finish the layers

    if (retval.isOk()) {
        if (m_image) {
            KisImageAnimationInterface *animation = m_image->animationInterface();

            if (frame > frameCount)
                frameCount = frame;

            animation->setDocumentRange(KisTimeSpan::fromTimeToTime(0,frameCount - 1));
            animation->setFramerate((int)framerate);
        }

        for (idx = 0; idx < layers.size(); idx++) {
            CSVLayerRecord* layer = layers.at(idx);
            //empty layers without any pictures are dropped

            if ((layer->frame > 0) || !layer->last.isEmpty()) {
                retval = setLayer(layer, importDoc.data(), path);

                if (!retval.isOk())
                    break;
            }
        }
    }

    if (m_image) {
        //insert the existing layers by the right order
        for (idx = layers.size() - 1; idx >= 0; idx--) {
            CSVLayerRecord* layer = layers.at(idx);

            if (layer->layer) {
                m_image->addNode(layer->layer, m_image->root());
            }
        }
        m_image->unlock();
    }
    for (CSVLayerRecord *layer : layers) {
        delete layer;
    }
    io->close();

    return retval;
}

PkString CSVLoader::convertBlending(const PkString &blending)
{
    if (blending == "Color") return COMPOSITE_OVER;
    if (blending == "Behind") return COMPOSITE_BEHIND;
    if (blending == "Erase") return COMPOSITE_ERASE;
    // "Shade"
    if (blending == "Light") return COMPOSITE_LINEAR_LIGHT;
    if (blending == "Colorize") return COMPOSITE_COLORIZE;
    if (blending == "Hue") return COMPOSITE_HUE;
    if (blending == "Add") return COMPOSITE_ADD;
    if (blending == "Sub") return COMPOSITE_INVERSE_SUBTRACT;
    if (blending == "Multiply") return COMPOSITE_MULT;
    if (blending == "Screen") return COMPOSITE_SCREEN;
    // "Replace"
    // "Substitute"
    if (blending == "Difference") return COMPOSITE_DIFF;
    if (blending == "Divide") return COMPOSITE_DIVIDE;
    if (blending == "Overlay") return COMPOSITE_OVERLAY;
    if (blending == "Light2") return COMPOSITE_DODGE;
    if (blending == "Shade2") return COMPOSITE_BURN;
    if (blending == "HardLight") return COMPOSITE_HARD_LIGHT;
    if (blending == "SoftLight") return COMPOSITE_SOFT_LIGHT_PHOTOSHOP;
    if (blending == "GrainExtract") return COMPOSITE_GRAIN_EXTRACT;
    if (blending == "GrainMerge") return COMPOSITE_GRAIN_MERGE;
    if (blending == "Sub2") return COMPOSITE_SUBTRACT;
    if (blending == "Darken") return COMPOSITE_DARKEN;
    if (blending == "Lighten") return COMPOSITE_LIGHTEN;
    if (blending == "Saturation") return COMPOSITE_SATURATION;

    return COMPOSITE_OVER;
}

PkString CSVLoader::validPath(const PkString &path,const PkString &base)
{
    std::string normalized = path.PkToUtf8();
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    while (!normalized.empty() && normalized.back() == '/') {
        normalized.pop_back();
    }

    const std::filesystem::path direct = std::filesystem::u8path(normalized);
    if (isDirectory(direct)) {
        return pathWithSlash(direct);
    }

    const std::filesystem::path basePath = std::filesystem::u8path(base.PkToUtf8());
    for (std::size_t slash = normalized.rfind('/'); slash != std::string::npos;) {
        const std::size_t previous = slash == 0 ? std::string::npos : normalized.rfind('/', slash - 1);
        const std::size_t componentStart = previous == std::string::npos ? 0 : previous + 1;
        const std::string parentComponent = normalized.substr(componentStart, slash - componentStart);

        if (parentComponent != ".layers") {
            const std::filesystem::path candidate = basePath / std::filesystem::u8path(normalized.substr(slash + 1));
            if (isDirectory(candidate)) {
                return pathWithSlash(candidate);
            }
        }

        if (slash == 0) break;
        slash = normalized.rfind('/', slash - 1);
    }
    return PkString();
}

KisImportExportErrorCode CSVLoader::setLayer(CSVLayerRecord* layer, KisDocument *importDoc, const PkString &path)
{
    bool result = true;

    if (layer->channel == 0) {
        //create a new document layer

        float opacity = layer->density;

        if (opacity > 1.0)
            opacity = 1.0;
        else if (opacity < 0.0)
            opacity = 0.0;

        const KoColorSpace* cs = m_image->colorSpace();
        const PkString layerName = (layer->name).isEmpty() ? m_image->nextLayerName() : layer->name;

        KisPaintLayer* paintLayer = new KisPaintLayer(m_image, layerName,
                                                       static_cast<std::uint8_t>(opacity * OPACITY_OPAQUE_U8), cs);

        paintLayer->setCompositeOpId(convertBlending(layer->blending));
        paintLayer->setVisible(layer->visible);
        paintLayer->enableAnimation();

        layer->layer = paintLayer;
        layer->channel = dynamic_cast<KisRasterKeyframeChannel *>(
            paintLayer->getKeyframeChannel(KisKeyframeChannel::Raster.id(), true));
    }


    if (!layer->last.isEmpty()) {
        //png image
        PkString filename = layer->hasPath ? layer->path : path;
        filename.append(layer->last);

        result = importDoc->openPath(filename,
                                    KisDocument::DontAddToRecent);
        if (result)
            layer->channel->importFrame(layer->frame, importDoc->image()->projection(), nullptr);

    } else {
        //blank
        layer->channel->addKeyframe(layer->frame);
    }
    return (result) ? ImportExportCodes::OK : ImportExportCodes::Failure;
}

KisImportExportErrorCode CSVLoader::createNewImage(int width, int height, float ratio, const PkString &name)
{
    //the CSV is RGBA 8bits, sRGB

    if (!m_image) {
        const KoColorSpace* cs = KoColorSpaceRegistry::instance()->colorSpace(
                                RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), 0);

        if (cs) m_image = new KisImage(m_doc->createUndoStore(), width, height, cs, name);

        if (!m_image) return ImportExportCodes::Failure;

        m_image->setResolution(ratio, 1.0);
        m_image->barrierLock();
    }
    return ImportExportCodes::OK;
}

KisImportExportErrorCode CSVLoader::buildAnimation(PkStream *io, const PkString &filename)
{
    return decode(io, filename);
}

KisImageSP CSVLoader::image()
{
    return m_image;
}

void CSVLoader::cancel()
{
    m_stop = true;
}
