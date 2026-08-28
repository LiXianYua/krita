/*
 *  SPDX-FileCopyrightText: 2005 Adrian Page <adrian@pagenet.plus.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "exr_converter.h"

#include <half.h>

#include <ImfAttribute.h>
#include <ImfChannelList.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#include <ImfOutputFile.h>

#include <ImfStringAttribute.h>
#include "exr_extra_tags.h"
#include "exr_import_policy.h"

#include <PkXmlDocument.h>
#include <PkThread.h>

#include <cassert>
#include <map>
#include <stdexcept>
#include <vector>

#include <KoColorSpaceRegistry.h>
#include <KoCompositeOpRegistry.h>
#include <KoColorSpaceTraits.h>
#include <KoColorModelStandardIds.h>
#include <KoColor.h>
#include <KoColorProfile.h>

#include <KisDocument.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_paint_layer.h>
#include "kis_iterator_ng.h"
#include <kis_exr_layers_sorter.h>

#include <kis_meta_data_entry.h>
#include <kis_meta_data_schema.h>
#include <kis_meta_data_schema_registry.h>
#include <kis_meta_data_store.h>
#include <kis_meta_data_value.h>

#include "kis_kra_savexml_visitor.h"

#include <KisImportExportAdditionalChecks.h>

// Do not translate!
#define HDR_LAYER "HDR Layer"

template<typename _T_>
struct Rgba {
    _T_ r;
    _T_ g;
    _T_ b;
    _T_ a;
};

struct ExrGroupLayerInfo;

struct ExrLayerInfoBase {
    ExrLayerInfoBase() : colorSpace(0), parent(0) {
    }
    const KoColorSpace* colorSpace;
    PkString name;
    const ExrGroupLayerInfo* parent;
};

struct ExrGroupLayerInfo : public ExrLayerInfoBase {
    ExrGroupLayerInfo() : groupLayer(0) {}
    KisGroupLayerSP groupLayer;
};

enum ImageType {
    IT_UNKNOWN,
    IT_FLOAT16,
    IT_FLOAT32,
    IT_UNSUPPORTED
};

struct ExrPaintLayerInfo : public ExrLayerInfoBase {
    ExrPaintLayerInfo()
        : imageType(IT_UNKNOWN)
    {
    }

    ImageType imageType;
    PkMap< PkString, PkString> channelMap; ///< first is either R, G, B or A second is the EXR channel name

    struct Remap {
        Remap(const PkString& _original, const PkString& _current) : original(_original), current(_current) {
        }
        PkString original;
        PkString current;
    };

    PkList< Remap > remappedChannels; ///< this is used to store in the metadata the mapping between exr channel name, and channels used in Krita
    void updateImageType(ImageType channelType);
};

void ExrPaintLayerInfo::updateImageType(ImageType channelType)
{
    if (imageType == IT_UNKNOWN) {
        imageType = channelType;
    }
    else if (imageType != channelType) {
        imageType = IT_UNSUPPORTED;
    }
}

struct ExrPaintLayerSaveInfo {
    PkString name; ///< name of the layer with a "." at the end (ie "group1.group2.layer1.")
    KisPaintDeviceSP layerDevice;
    KisPaintLayerSP layer;
    PkList<PkString> channels;
    Imf::PixelType pixelType;
};

struct EXRConverter::Private {
    Private()
        : doc(0)
        , alphaWasModified(false)
        , showNotifications(false)
    {}

    KisImageSP image;
    KisDocument *doc;

    bool alphaWasModified;
    bool showNotifications;

    PkString errorMessage;

    template <class WrapperType>
    void unmultiplyAlpha(typename WrapperType::pixel_type *pixel);

    template<typename _T_>
    void decodeData4(Imf::InputFile& file, ExrPaintLayerInfo& info, KisPaintLayerSP layer, int width, int xstart, int ystart, int height, Imf::PixelType ptype);

    template<typename _T_>
    void decodeData1(Imf::InputFile& file, ExrPaintLayerInfo& info, KisPaintLayerSP layer, int width, int xstart, int ystart, int height, Imf::PixelType ptype);


    PkXmlDocument loadExtraLayersInfo(const Imf::Header &header);
    bool checkExtraLayersInfoConsistent(const PkXmlDocument &doc, std::set<std::string> exrLayerNames);
    void makeLayerNamesUnique(PkList<ExrPaintLayerSaveInfo>& informationObjects);
    void recBuildPaintLayerSaveInfo(PkList<ExrPaintLayerSaveInfo>& informationObjects, const PkString& name, KisGroupLayerSP parent);
    void reportLayersNotSaved(const PkSet<KisNodeSP> &layersNotSaved);
    PkString fetchExtraLayersInfo(PkList<ExrPaintLayerSaveInfo>& informationObjects);
};

EXRConverter::EXRConverter(KisDocument *doc, bool showNotifications)
    : d(new Private)
{
    d->doc = doc;
    d->showNotifications = showNotifications;

    // Set thread count for IlmImf library
    Imf::setGlobalThreadCount(PkThread::idealThreadCount());
    dbgFile << "EXR Threadcount was set to: " << PkThread::idealThreadCount();
}

EXRConverter::~EXRConverter()
{
}

ImageType imfTypeToKisType(Imf::PixelType type)
{
    switch (type) {
    case Imf::UINT:
    case Imf::NUM_PIXELTYPES:
        return IT_UNSUPPORTED;
    case Imf::HALF:
        return IT_FLOAT16;
    case Imf::FLOAT:
        return IT_FLOAT32;
    default:
        throw std::runtime_error("OpenEXR returned an out-of-range pixel type");
    }
}

const KoColorSpace *kisTypeToColorSpace(PkString colorModelID, ImageType imageType)
{

    PkString colorDepthID = "UNKNOWN";
    switch(imageType) {
    case IT_FLOAT16:
        colorDepthID = Float16BitsColorDepthID.id();
        break;
    case IT_FLOAT32:
        colorDepthID = Float32BitsColorDepthID.id();
        break;
    default:
        return 0;
    };

    const PkString colorSpaceId = KoColorSpaceRegistry::instance()->colorSpaceId(colorModelID, colorDepthID);
    const PkString defaultProfileForColorSpace = KoColorSpaceRegistry::instance()->defaultProfileForColorSpace(colorSpaceId);

    /**
     * Our user settings are only for the RGB color model, for other models just use
     * the default one provided by the color space.
     */
    const PkString profileName = colorModelID == RGBAColorModelID.id()
        ? preferredExrColorProfile(defaultProfileForColorSpace)
        : defaultProfileForColorSpace;

    return KoColorSpaceRegistry::instance()->colorSpace(colorModelID, colorDepthID, profileName);

}

template <typename T>
static inline T alphaEpsilon()
{
    return static_cast<T>(HALF_EPSILON);
}

template <typename T>
static inline T alphaNoiseThreshold()
{
    return static_cast<T>(0.01); // 1%
}

static inline bool qFuzzyCompare(half p1, half p2)
{
    return std::abs(p1 - p2) < float(HALF_EPSILON);
}

static inline bool qFuzzyIsNull(half h)
{
    return std::abs(h) < float(HALF_EPSILON);
}

template <typename T>
struct RgbPixelWrapper
{
    typedef T channel_type;
    typedef Rgba<T> pixel_type;

    RgbPixelWrapper(Rgba<T> &_pixel) : pixel(_pixel) {}

    inline T alpha() const {
        return pixel.a;
    }

    inline bool checkMultipliedColorsConsistent() const {
        return !(std::abs(pixel.a) <= alphaEpsilon<T>() &&
                 (!qFuzzyIsNull(pixel.r) ||
                  !qFuzzyIsNull(pixel.g) ||
                  !qFuzzyIsNull(pixel.b)));
    }

    inline bool checkUnmultipliedColorsConsistent(const Rgba<T> &mult) const {
        const T alpha = std::abs(pixel.a);

        return alpha >= alphaNoiseThreshold<T>() ||
                (qFuzzyCompare(T(pixel.r * alpha), mult.r) &&
                 qFuzzyCompare(T(pixel.g * alpha), mult.g) &&
                 qFuzzyCompare(T(pixel.b * alpha), mult.b));
    }

    inline void setUnmultiplied(const Rgba<T> &mult, T newAlpha) {
        const T absoluteAlpha = std::abs(newAlpha);

        pixel.r = mult.r / absoluteAlpha;
        pixel.g = mult.g / absoluteAlpha;
        pixel.b = mult.b / absoluteAlpha;
        pixel.a = newAlpha;
    }

    Rgba<T> &pixel;
};

template <typename T>
struct GrayPixelWrapper
{
    typedef T channel_type;
    typedef typename KoGrayTraits<T>::Pixel pixel_type;

    GrayPixelWrapper(pixel_type &_pixel) : pixel(_pixel) {}

    inline T alpha() const {
        return pixel.alpha;
    }

    inline bool checkMultipliedColorsConsistent() const {
        return !(std::abs(pixel.alpha) <= alphaEpsilon<T>() &&
                 !qFuzzyIsNull(pixel.gray));
    }

    inline bool checkUnmultipliedColorsConsistent(const pixel_type &mult) const {
        const T alpha = std::abs(pixel.alpha);

        return alpha >= alphaNoiseThreshold<T>() ||
                qFuzzyCompare(T(pixel.gray * alpha), mult.gray);
    }

    inline void setUnmultiplied(const pixel_type &mult, T newAlpha) {
        const T absoluteAlpha = std::abs(newAlpha);

        pixel.gray = mult.gray / absoluteAlpha;
        pixel.alpha = newAlpha;
    }

    pixel_type &pixel;
};

template <class WrapperType>
void EXRConverter::Private::unmultiplyAlpha(typename WrapperType::pixel_type *pixel)
{
    typedef typename WrapperType::pixel_type pixel_type;
    typedef typename WrapperType::channel_type channel_type;

    WrapperType srcPixel(*pixel);

    if (!srcPixel.checkMultipliedColorsConsistent()) {

        channel_type newAlpha = srcPixel.alpha();

        pixel_type __dstPixelData;
        WrapperType dstPixel(__dstPixelData);

        /**
         * Division by a tiny alpha may result in an overflow of half
         * value. That is why we use safe iterative approach.
         */
        while (1) {
            dstPixel.setUnmultiplied(srcPixel.pixel, newAlpha);

            if (dstPixel.checkUnmultipliedColorsConsistent(srcPixel.pixel)) {
                break;
            }

            newAlpha += alphaEpsilon<channel_type>();
            alphaWasModified = true;
        }

        *pixel = dstPixel.pixel;


    } else if (srcPixel.alpha() > 0.0) {
        srcPixel.setUnmultiplied(srcPixel.pixel, srcPixel.alpha());
    }
}

template <typename T, typename Pixel, int size, int alphaPos>
void multiplyAlpha(Pixel *pixel)
{
    if (alphaPos >= 0) {
        T alpha = pixel->data[alphaPos];

        if (alpha > alphaEpsilon<T>()) {
            for (int i = 0; i < size; ++i) {
                if (i != alphaPos) {
                    pixel->data[i] *= alpha;
                }
            }

            pixel->data[alphaPos] = alpha;
        } else {
            for (int i = 0; i < size; ++i) {
                pixel->data[i] = 0;
            }
        }
    }
}

template<typename _T_>
void EXRConverter::Private::decodeData4(Imf::InputFile& file, ExrPaintLayerInfo& info, KisPaintLayerSP layer, int width, int xstart, int ystart, int height, Imf::PixelType ptype)
{
    typedef Rgba<_T_> Rgba;

    PkVector<Rgba> pixels(width * height);

    bool hasAlpha = info.channelMap.contains("A");

    Imf::FrameBuffer frameBuffer;
    Rgba* frameBufferData = (pixels.data()) - xstart - ystart * width;
    frameBuffer.insert(info.channelMap["R"].PkToUtf8().c_str(),
            Imf::Slice(ptype, (char *) &frameBufferData->r,
                       sizeof(Rgba) * 1,
                       sizeof(Rgba) * width));
    frameBuffer.insert(info.channelMap["G"].PkToUtf8().c_str(),
            Imf::Slice(ptype, (char *) &frameBufferData->g,
                       sizeof(Rgba) * 1,
                       sizeof(Rgba) * width));
    frameBuffer.insert(info.channelMap["B"].PkToUtf8().c_str(),
            Imf::Slice(ptype, (char *) &frameBufferData->b,
                       sizeof(Rgba) * 1,
                       sizeof(Rgba) * width));
    if (hasAlpha) {
        frameBuffer.insert(info.channelMap["A"].PkToUtf8().c_str(),
                Imf::Slice(ptype, (char *) &frameBufferData->a,
                           sizeof(Rgba) * 1,
                           sizeof(Rgba) * width));
    }

    file.setFrameBuffer(frameBuffer);
    file.readPixels(ystart, height + ystart - 1);
    Rgba *rgba = pixels.data();

    PkRect paintRegion(xstart, ystart, width, height);
    KisSequentialIterator it(layer->paintDevice(), paintRegion);
    while (it.nextPixel()) {
        if (hasAlpha) {
            unmultiplyAlpha<RgbPixelWrapper<_T_> >(rgba);
        }

        typename KoRgbTraits<_T_>::Pixel* dst = reinterpret_cast<typename KoRgbTraits<_T_>::Pixel*>(it.rawData());

        dst->red = rgba->r;
        dst->green = rgba->g;
        dst->blue = rgba->b;
        if (hasAlpha) {
            dst->alpha = rgba->a;
        } else {
            dst->alpha = 1.0;
        }

        ++rgba;
    }
}

template<typename _T_>
void EXRConverter::Private::decodeData1(Imf::InputFile& file, ExrPaintLayerInfo& info, KisPaintLayerSP layer, int width, int xstart, int ystart, int height, Imf::PixelType ptype)
{
    typedef typename GrayPixelWrapper<_T_>::channel_type channel_type;
    typedef typename GrayPixelWrapper<_T_>::pixel_type pixel_type;

    KIS_ASSERT_RECOVER_RETURN(
                layer->paintDevice()->colorSpace()->colorModelId() == GrayAColorModelID);

    PkVector<pixel_type> pixels(width * height);

    assert(info.channelMap.contains("Y"));
    dbgFile << "Gray -> " << info.channelMap["Y"];

    bool hasAlpha = info.channelMap.contains("A");
    dbgFile << "Has Alpha:" << hasAlpha;


    Imf::FrameBuffer frameBuffer;
    pixel_type* frameBufferData = (pixels.data()) - xstart - ystart * width;
    frameBuffer.insert(
        info.channelMap["Y"].PkToUtf8().c_str(),
        Imf::Slice(ptype, (char *)&frameBufferData->gray, sizeof(pixel_type) * 1, sizeof(pixel_type) * width));

    if (hasAlpha) {
        frameBuffer.insert(info.channelMap["A"].PkToUtf8().c_str(),
                Imf::Slice(ptype, (char *) &frameBufferData->alpha,
                           sizeof(pixel_type) * 1,
                           sizeof(pixel_type) * width));
    }

    file.setFrameBuffer(frameBuffer);
    file.readPixels(ystart, height + ystart - 1);

    pixel_type *srcPtr = pixels.data();

    PkRect paintRegion(xstart, ystart, width, height);
    KisSequentialIterator it(layer->paintDevice(), paintRegion);
    while (it.nextPixel()) {
        if (hasAlpha) {
            unmultiplyAlpha<GrayPixelWrapper<_T_> >(srcPtr);
        }

        pixel_type* dstPtr = reinterpret_cast<pixel_type*>(it.rawData());

        dstPtr->gray = srcPtr->gray;
        dstPtr->alpha = hasAlpha ? srcPtr->alpha : channel_type(1.0);

        ++srcPtr;
    } ;
}

bool recCheckGroup(const ExrGroupLayerInfo& group, const std::vector<PkString> &list, int idx1, int idx2)
{
    if (idx1 > idx2) return true;
    if (group.name == list[idx2]) {
        return recCheckGroup(*group.parent, list, idx1, idx2 - 1);
    }
    return false;
}

ExrGroupLayerInfo* searchGroup(PkList<ExrGroupLayerInfo>* groups, const std::vector<PkString> &list, int idx1, int idx2)
{
    if (idx1 > idx2) {
        return 0;
    }
    // Look for the group
    for (int i = 0; i < groups->size(); ++i) {
        if (recCheckGroup(groups->at(i), list, idx1, idx2)) {
            return &(*groups)[i];
        }
    }
    // Create the group
    ExrGroupLayerInfo info;
    info.name = list.at(idx2);
    info.parent = searchGroup(groups, list, idx1, idx2 - 1);
    groups->append(info);
    return &groups->last();
}

PkXmlDocument EXRConverter::Private::loadExtraLayersInfo(const Imf::Header &header)
{
    const Imf::StringAttribute *layersInfoAttribute =
            header.findTypedAttribute<Imf::StringAttribute>(EXR_KRITA_LAYERS);

    if (!layersInfoAttribute) return PkXmlDocument();

    PkString layersInfoString(layersInfoAttribute->value().c_str());

    PkXmlDocument doc;
    doc.setContent(layersInfoString);

    return doc;
}

bool EXRConverter::Private::checkExtraLayersInfoConsistent(const PkXmlDocument &doc, std::set<std::string> exrLayerNames)
{
    std::set<std::string> extraInfoLayers;

    PkXmlElement root = doc.documentElement();

    KIS_ASSERT_RECOVER(!root.isNull() && root.hasChildNodes()) { return false; };

    PkXmlElement el = root.firstChildElement();

    while(!el.isNull()) {
        KIS_ASSERT_RECOVER(el.hasAttribute(EXR_NAME)) { return false; };
        const std::string layerName = el.attribute(EXR_NAME).PkToUtf8();
        if (layerName != HDR_LAYER) {
            extraInfoLayers.insert(layerName);
        }
        el = el.nextSiblingElement();
    }

    bool result = (extraInfoLayers == exrLayerNames);

    if (!result) {
        dbgKrita << "WARNING: Krita EXR extra layers info is inconsistent!";
        dbgKrita << ppVar(extraInfoLayers.size()) << ppVar(exrLayerNames.size());

        std::set<std::string>::const_iterator it1 = extraInfoLayers.begin();
        std::set<std::string>::const_iterator it2 = exrLayerNames.begin();

        std::set<std::string>::const_iterator end1 = extraInfoLayers.end();

        for (; it1 != end1; ++it1, ++it2) {
            dbgKrita << it1->c_str() << it2->c_str();
        }

    }

    return result;
}

KisImportExportErrorCode EXRConverter::decode(const PkString &filename)
{
    try {
        Imf::InputFile file(filename.PkToUtf8().c_str());

        Imath::Box2i dw = file.header().dataWindow();
        Imath::Box2i displayWindow = file.header().displayWindow();

        int width = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;
        int dx = dw.min.x;
        int dy = dw.min.y;

        // Display the attributes of a file
        for (Imf::Header::ConstIterator it = file.header().begin();
             it != file.header().end(); ++it) {
            dbgFile << "Attribute: " << it.name() << " type: " << it.attribute().typeName();
        }

        // fetch Krita's extra layer info, which might have been stored previously
        const bool hasExtraLayersAttribute =
            file.header().findTypedAttribute<Imf::StringAttribute>(EXR_KRITA_LAYERS) != nullptr;
        PkXmlDocument extraLayersInfo = d->loadExtraLayersInfo(file.header());
        bool useExtraLayersInfo =
            hasUsableExrLayersMetadata(hasExtraLayersAttribute, extraLayersInfo);

        // Construct the list of LayerInfo

        PkList<ExrPaintLayerInfo> informationObjects;
        PkList<ExrGroupLayerInfo> groups;

        ImageType imageType = IT_UNKNOWN;

        const Imf::ChannelList &channels = file.header().channels();
        std::set<std::string> layerNames;
        channels.layers(layerNames);

        if (useExtraLayersInfo &&
                !d->checkExtraLayersInfoConsistent(extraLayersInfo, layerNames)) {

            // it is inconsistent anyway
            useExtraLayersInfo = false;
        }

        // Check if there are A, R, G, B channels

        dbgFile << "Checking for ARGB channels, they can occur in single-layer _or_ multi-layer images:";
        ExrPaintLayerInfo info;
        bool topLevelRGBFound = false;
        info.name = HDR_LAYER;

        PkStringList topLevelChannelNames = PkStringList() << "A"
                                                         << "R"
                                                         << "G"
                                                         << "B"
                                                         << ".A"
                                                         << ".R"
                                                         << ".G"
                                                         << ".B"
                                                         << "A."
                                                         << "R."
                                                         << "G."
                                                         << "B."
                                                         << "A."
                                                         << "R."
                                                         << "G."
                                                         << "B."
                                                         << ".alpha"
                                                         << ".red"
                                                         << ".green"
                                                         << ".blue"
                                                         << "X"
                                                         << "Y"
                                                         << "Z"
                                                         << ".X"
                                                         << ".Y"
                                                         << ".Z"
                                                         << "X."
                                                         << "Y."
                                                         << "Z.";

        for (Imf::ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i) {
            const Imf::Channel &channel = i.channel();
            dbgFile << "Channel name = " << i.name() << " type = " << channel.type;

            PkString qname = i.name();
            if (topLevelChannelNames.contains(qname)) {
                topLevelRGBFound = true;
                dbgFile << "Found top-level channel" << qname;
                info.channelMap[qname] = qname;
                info.updateImageType(imfTypeToKisType(channel.type));
            }
            // Channel names that don't contain a "." or that contain a
            // "." only at the beginning or at the end are not considered
            // to be part of any layer.
            else if (!qname.contains(PkString("."))
                     || !qname.mid(1).contains(PkString("."))
                     || !qname.left(qname.size() - 1).contains(PkString("."))) {
                warnFile << "Found a top-level channel that is not part of the rendered image" << qname << ". Krita will not load this channel.";
            }
        }
        if (topLevelRGBFound) {
            dbgFile << "Toplevel layer" << info.name << ":Image type:" << imageType << "Layer type" << info.imageType;
            informationObjects.push_back(info);
            imageType = info.imageType;
        }

        dbgFile << "Extra layers:" << layerNames.size();

        for (std::set<std::string>::const_iterator i = layerNames.begin();i != layerNames.end(); ++i) {

            info = ExrPaintLayerInfo();

            dbgFile << "layer name = " << i->c_str();
            info.name = i->c_str();
            Imf::ChannelList::ConstIterator layerBegin, layerEnd;
            channels.channelsInLayer(*i, layerBegin, layerEnd);
            for (Imf::ChannelList::ConstIterator j = layerBegin;
                 j != layerEnd; ++j) {
                const Imf::Channel &channel = j.channel();

                info.updateImageType(imfTypeToKisType(channel.type));

                PkString qname = j.name();
                const std::vector<PkString> list = qname.split(u'.');
                PkString layersuffix = list.back();

                dbgFile << "\tchannel " << j.name() << "suffix" << layersuffix << " type = " << channel.type;

                // Nuke writes the channels for sublayers as .red instead of .R, so convert those.
                // See https://bugs.kde.org/show_bug.cgi?id=393771
                if (topLevelChannelNames.contains(PkString(".") + layersuffix)) {
                    layersuffix = layersuffix.left(1).toUpper();
                }
                dbgFile << "\t\tsuffix" << layersuffix;


                if (list.size() > 1) {
                    info.name = list[list.size()-2];
                    info.parent = searchGroup(&groups, list, 0, list.size() - 3);
                }

                info.channelMap[layersuffix] = qname;
            }

            if (info.imageType != IT_UNKNOWN && info.imageType != IT_UNSUPPORTED) {
                informationObjects.push_back(info);
                if (imageType < info.imageType) {
                    imageType = info.imageType;
                }
            }
        }

        dbgFile << "File has" << informationObjects.size() << "layer(s)";

        // Set the colorspaces
        for (int i = 0; i < informationObjects.size(); ++i) {
            ExrPaintLayerInfo& info = informationObjects[i];
            PkString modelId;

            std::set<std::string> channelKeys;
            for (auto it = info.channelMap.constBegin(); it != info.channelMap.constEnd(); ++it) {
                channelKeys.insert(it.key().PkToUtf8());
            }
            const ExrChannelModel channelModel = classifyExrChannels(channelKeys);

            if (channelModel == ExrChannelModel::Gray && info.channelMap.size() == 1) {
                modelId = GrayAColorModelID.id();
                PkString key = info.channelMap.begin().key();
                if (key != "Y") {
                    info.remappedChannels.push_back(ExrPaintLayerInfo::Remap(key, "Y"));
                    PkString channel =  info.channelMap.begin().value();
                    info.channelMap.clear();
                    info.channelMap["Y"] = channel;
                }
            }
            else if (channelModel == ExrChannelModel::Gray && info.channelMap.size() == 2) {
                modelId = GrayAColorModelID.id();

                PkMap<PkString,PkString>::const_iterator it = info.channelMap.constBegin();
                PkMap<PkString,PkString>::const_iterator end = info.channelMap.constEnd();

                PkString failingChannelKey;

                for (; it != end; ++it) {
                    // BUG: 461975
                    if (it.key() != "A") {
                        failingChannelKey = it.key();
                        break;
                    }
                }

                info.remappedChannels.push_back(ExrPaintLayerInfo::Remap(failingChannelKey, "Y"));

                PkString failingChannelValue = info.channelMap[failingChannelKey];
                info.channelMap.remove(failingChannelKey);
                info.channelMap["Y"] = failingChannelValue;
            }
            else if (info.channelMap.size() == 3 || info.channelMap.size() == 4) {

                if (channelModel == ExrChannelModel::Rgb) {
                    modelId = RGBAColorModelID.id();
                }
                else if (channelModel == ExrChannelModel::Xyz) {
                    modelId = XYZAColorModelID.id();
                    PkMap<PkString, PkString> newChannelMap;
                    if (info.channelMap.contains("W")) {
                        newChannelMap["A"] = info.channelMap["W"];
                        info.remappedChannels.push_back(ExrPaintLayerInfo::Remap("W", "A"));
                        info.remappedChannels.push_back(ExrPaintLayerInfo::Remap("X", "X"));
                        info.remappedChannels.push_back(ExrPaintLayerInfo::Remap("Y", "Y"));
                        info.remappedChannels.push_back(ExrPaintLayerInfo::Remap("Z", "Z"));
                    } else if (info.channelMap.contains("A")) {
                        newChannelMap["A"] = info.channelMap["A"];
                    }
                    // The decode function expect R, G, B in the channel map
                    newChannelMap["R"] = info.channelMap["X"];
                    newChannelMap["G"] = info.channelMap["Y"];
                    newChannelMap["B"] = info.channelMap["Z"];
                    info.channelMap = newChannelMap;
                }
                else {
                    modelId = RGBAColorModelID.id();
                    PkMap<PkString, PkString> newChannelMap;
                    PkMap<PkString, PkString>::iterator it = info.channelMap.begin();
                    newChannelMap["R"] = it.value();
                    info.remappedChannels.push_back(ExrPaintLayerInfo::Remap(it.key(), "R"));
                    ++it;
                    newChannelMap["G"] = it.value();
                    info.remappedChannels.push_back(ExrPaintLayerInfo::Remap(it.key(), "G"));
                    ++it;
                    newChannelMap["B"] = it.value();
                    info.remappedChannels.push_back(ExrPaintLayerInfo::Remap(it.key(), "B"));
                    if (info.channelMap.size() == 4) {
                        ++it;
                        newChannelMap["A"] = it.value();
                        info.remappedChannels.push_back(ExrPaintLayerInfo::Remap(it.key(), "A"));
                    }

                    info.channelMap = newChannelMap;
                }
            }
            else {
                dbgFile << info.name << "has" << info.channelMap.size() << "channels, and we don't know what to do.";
            }
            if (!modelId.isEmpty()) {
                info.colorSpace = kisTypeToColorSpace(modelId, info.imageType);
            }
        }

        // Get colorspace
        dbgFile << "Image type = " << imageType;
        const KoColorSpace* colorSpace = kisTypeToColorSpace(RGBAColorModelID.id(), imageType);

        if (!colorSpace) return ImportExportCodes::FormatColorSpaceUnsupported;
        dbgFile << "Color space: " << colorSpace->name();

        // Set the colorspace on all groups
        for (int i = 0; i < groups.size(); ++i) {
            ExrGroupLayerInfo& info = groups[i];
            info.colorSpace = colorSpace;
        }

        // Create the image
        //  Make sure the created image is the same size as the displayWindow since
        //  the dataWindow can be cropped in some cases.
        int displayWidth = displayWindow.max.x - displayWindow.min.x + 1;
        int displayHeight = displayWindow.max.y - displayWindow.min.y + 1;
        d->image = new KisImage(d->doc->createUndoStore(), displayWidth, displayHeight, colorSpace, PkString());

        if (!d->image) {
            return ImportExportCodes::Failure;
        }

        /**
         * EXR semi-transparent images are expected to be rendered on
         * black to ensure correctness of the light model
         *
         * NOTE: We cannot do that automatically, because the EXR may be imported
         * into the image as a layer, in which case the default color will create
         * major issues. See https://bugs.kde.org/show_bug.cgi?id=427720
         */

        // Create group layers
        for (int i = 0; i < groups.size(); ++i) {
            ExrGroupLayerInfo& info = groups[i];
            assert(info.parent == 0 || info.parent->groupLayer);
            KisGroupLayerSP groupLayerParent = (info.parent) ? info.parent->groupLayer : d->image->rootLayer();
            info.groupLayer = new KisGroupLayer(d->image, info.name, OPACITY_OPAQUE_U8);
            d->image->addNode(info.groupLayer, groupLayerParent);
        }

        // Load the layers
        for (int i = informationObjects.size() - 1; i >= 0; --i) {
            ExrPaintLayerInfo& info = informationObjects[i];
            if (info.colorSpace) {
                dbgFile << "Decoding " << info.name << " with " << info.channelMap.size() << " channels, and color space " << info.colorSpace->id();
                KisPaintLayerSP layer = new KisPaintLayer(d->image, info.name, OPACITY_OPAQUE_U8, info.colorSpace);

                if (!layer) {
                    return ImportExportCodes::Failure;
                }

                layer->setCompositeOpId(COMPOSITE_OVER);

                switch (info.channelMap.size()) {
                case 1:
                case 2:
                    // Decode the data
                    switch (info.imageType) {
                    case IT_FLOAT16:
                        d->decodeData1<half>(file, info, layer, width, dx, dy, height, Imf::HALF);
                        break;
                    case IT_FLOAT32:
                        d->decodeData1<float>(file, info, layer, width, dx, dy, height, Imf::FLOAT);
                        break;
                    case IT_UNKNOWN:
                    case IT_UNSUPPORTED:
                        throw std::runtime_error("unsupported EXR gray channel type");
                    }
                    break;
                case 3:
                case 4:
                    // Decode the data
                    switch (info.imageType) {
                    case IT_FLOAT16:
                        d->decodeData4<half>(file, info, layer, width, dx, dy, height, Imf::HALF);
                        break;
                    case IT_FLOAT32:
                        d->decodeData4<float>(file, info, layer, width, dx, dy, height, Imf::FLOAT);
                        break;
                    case IT_UNKNOWN:
                    case IT_UNSUPPORTED:
                        throw std::runtime_error("unsupported EXR color channel type");
                    }
                    break;
                default:
                    throw std::runtime_error("invalid EXR channel count");
                }
                // Check if should set the channels
                if (!info.remappedChannels.isEmpty()) {
                    PkList<KisMetaData::Value> values;
                    for (const ExrPaintLayerInfo::Remap& remap : info.remappedChannels) {
                        PkMap<PkString, KisMetaData::Value> map;
                        map["original"] = KisMetaData::Value(remap.original);
                        map["current"] = KisMetaData::Value(remap.current);
                        values.append(map);
                    }
                    layer->metaData()->addEntry(KisMetaData::Entry(KisMetaData::SchemaRegistry::instance()->create("http://krita.org/exrchannels/1.0/" , "exrchannels"), "channelsmap", values));
                }
                // Add the layer
                KisGroupLayerSP groupLayerParent = (info.parent) ? info.parent->groupLayer : d->image->rootLayer();
                d->image->addNode(layer, groupLayerParent);
            } else {
                dbgFile << "No decoding " << info.name << " with " << info.channelMap.size() << " channels, and lack of a color space";
            }
        }

        // After reading the image, notify the user about changed alpha.
        if (d->alphaWasModified) {
            PkString msg(
                "The image contains pixels with zero alpha and non-zero color channels. "
                "Krita adjusted those pixels to have a minimal alpha; the original values "
                "will not be restored when the image is saved again.");
            if (d->showNotifications) {
                d->errorMessage = msg;
            } else {
                warnKrita << "WARNING:" << msg;
            }
        }

        if (useExtraLayersInfo) {
            KisExrLayersSorter sorter(extraLayersInfo, d->image);
        }

        return ImportExportCodes::OK;

    } catch (std::exception &e) {
        dbgFile << "Error while reading from the exr file: " << e.what();

        if (!KisImportExportAdditionalChecks::doesFileExist(filename)) {
            return ImportExportCodes::FileNotExist;
        } else if(!KisImportExportAdditionalChecks::isFileReadable(filename)) {
            return ImportExportCodes::NoAccessToRead;
        } else {
            return ImportExportCodes::ErrorWhileReading;
        }
    }

    return ImportExportCodes::OK;
}

KisImportExportErrorCode EXRConverter::buildImage(const PkString &filename)
{
    return decode(filename);

}


KisImageSP EXRConverter::image()
{
    return d->image;
}

PkString EXRConverter::errorMessage() const
{
    return d->errorMessage;
}

template<typename _T_, int size>
struct ExrPixel_ {
    _T_ data[size];
};

class Encoder
{
public:
    virtual ~Encoder() {}
    virtual void prepareFrameBuffer(Imf::FrameBuffer*, int line) = 0;
    virtual void encodeData(int line) = 0;

};

template<typename _T_, int size, int alphaPos>
class EncoderImpl : public Encoder
{
public:
    EncoderImpl(Imf::OutputFile* _file, const ExrPaintLayerSaveInfo* _info, int width) : file(_file), info(_info), pixels(width), m_width(width) {}
    ~EncoderImpl() override {}
    void prepareFrameBuffer(Imf::FrameBuffer*, int line) override;
    void encodeData(int line) override;
private:
    typedef ExrPixel_<_T_, size> ExrPixel;
    Imf::OutputFile* file;
    const ExrPaintLayerSaveInfo* info;
    PkVector<ExrPixel> pixels;
    int m_width;
};

template<typename _T_, int size, int alphaPos>
void EncoderImpl<_T_, size, alphaPos>::prepareFrameBuffer(Imf::FrameBuffer* frameBuffer, int line)
{
    int xstart = 0;
    int ystart = 0;
    ExrPixel* frameBufferData = (pixels.data()) - xstart - (ystart + line) * m_width;
    for (int k = 0; k < size; ++k) {
        frameBuffer->insert(info->channels[k].PkToUtf8().c_str(),
                            Imf::Slice(info->pixelType, (char *) &frameBufferData->data[k],
                                       sizeof(ExrPixel) * 1,
                                       sizeof(ExrPixel) * m_width));
    }
}

template<typename _T_, int size, int alphaPos>
void EncoderImpl<_T_, size, alphaPos>::encodeData(int line)
{
    ExrPixel *rgba = pixels.data();
    KisHLineConstIteratorSP it = info->layerDevice->createHLineConstIteratorNG(0, line, m_width);
    do {
        const _T_* dst = reinterpret_cast < const _T_* >(it->oldRawData());

        for (int i = 0; i < size; ++i) {
            rgba->data[i] = dst[i];
        }

        if (alphaPos != -1) {
            multiplyAlpha<_T_, ExrPixel, size, alphaPos>(rgba);
        }

        ++rgba;
    } while (it->nextPixel());
}

Encoder* encoder(Imf::OutputFile& file, const ExrPaintLayerSaveInfo& info, int width)
{
    dbgFile << "Create encoder for" << info.name << info.channels << info.layerDevice->colorSpace()->channelCount();
    switch (info.layerDevice->colorSpace()->channelCount()) {
    case 1: {
        if (info.layerDevice->colorSpace()->colorDepthId() == Float16BitsColorDepthID) {
            assert(info.pixelType == Imf::HALF);
            return new EncoderImpl < half, 1, -1 > (&file, &info, width);
        } else if (info.layerDevice->colorSpace()->colorDepthId() == Float32BitsColorDepthID) {
            assert(info.pixelType == Imf::FLOAT);
            return new EncoderImpl < float, 1, -1 > (&file, &info, width);
        }
        break;
    }
    case 2: {
        if (info.layerDevice->colorSpace()->colorDepthId() == Float16BitsColorDepthID) {
            assert(info.pixelType == Imf::HALF);
            return new EncoderImpl<half, 2, 1>(&file, &info, width);
        } else if (info.layerDevice->colorSpace()->colorDepthId() == Float32BitsColorDepthID) {
            assert(info.pixelType == Imf::FLOAT);
            return new EncoderImpl<float, 2, 1>(&file, &info, width);
        }
        break;
    }
    case 4: {
        if (info.layerDevice->colorSpace()->colorDepthId() == Float16BitsColorDepthID) {
            assert(info.pixelType == Imf::HALF);
            return new EncoderImpl<half, 4, 3>(&file, &info, width);
        } else if (info.layerDevice->colorSpace()->colorDepthId() == Float32BitsColorDepthID) {
            assert(info.pixelType == Imf::FLOAT);
            return new EncoderImpl<float, 4, 3>(&file, &info, width);
        }
        break;
    }
    default:
        throw std::runtime_error("unsupported EXR encoder channel count");
    }
    return 0;
}

void encodeData(Imf::OutputFile& file, const PkList<ExrPaintLayerSaveInfo>& informationObjects, int width, int height)
{
    PkList<Encoder*> encoders;
    for (const ExrPaintLayerSaveInfo& info : informationObjects) {
        encoders.push_back(encoder(file, info, width));
    }

    for (int y = 0; y < height; ++y) {
        Imf::FrameBuffer frameBuffer;
        for (Encoder* encoder : encoders) {
            encoder->prepareFrameBuffer(&frameBuffer, y);
        }
        file.setFrameBuffer(frameBuffer);
        for (Encoder* encoder : encoders) {
            encoder->encodeData(y);
        }
        file.writePixels(1);
    }
    for (Encoder *encoder : encoders) {
        delete encoder;
    }
}

KisPaintDeviceSP wrapLayerDevice(KisPaintDeviceSP device)
{
    const KoColorSpace *cs = device->colorSpace();

    if (cs->colorDepthId() != Float16BitsColorDepthID && cs->colorDepthId() != Float32BitsColorDepthID) {
        /**
         * We should try to keep the same profile of the space when possible
         * (i.e. when the color model is kept the same)
         */
        const KoColorProfile *targetBestEffortProfile = nullptr;
        if (cs->colorModelId() == GrayAColorModelID ||
            cs->colorModelId() == RGBAColorModelID) {
            targetBestEffortProfile = cs->profile();
        }

        cs = KoColorSpaceRegistry::instance()->colorSpace(
            cs->colorModelId() == GrayAColorModelID ?
                GrayAColorModelID.id() : RGBAColorModelID.id(),
            Float16BitsColorDepthID.id(),
            targetBestEffortProfile);
    } else if (cs->colorModelId() != GrayAColorModelID &&
               cs->colorModelId() != RGBAColorModelID) {
        cs = KoColorSpaceRegistry::instance()->colorSpace(
            RGBAColorModelID.id(),
            cs->colorDepthId().id());
    }

    if (*cs != *device->colorSpace()) {
        device = new KisPaintDevice(*device);
        device->convertTo(cs);
    }

    return device;
}

KisImportExportErrorCode EXRConverter::buildFile(const PkString &filename, KisPaintLayerSP layer)
{
    KIS_ASSERT_RECOVER_RETURN_VALUE(layer, ImportExportCodes::InternalError);

    KisImageSP image = layer->image();
    KIS_ASSERT_RECOVER_RETURN_VALUE(image, ImportExportCodes::InternalError);


    // Make the header
    qint32 height = image->height();
    qint32 width = image->width();
    Imf::Header header(width, height);

    ExrPaintLayerSaveInfo info;
    info.layer = layer;
    info.layerDevice = wrapLayerDevice(layer->paintDevice());
    Imf::PixelType pixelType = Imf::NUM_PIXELTYPES;
    if (info.layerDevice->colorSpace()->colorDepthId() == Float16BitsColorDepthID) {
        pixelType = Imf::HALF;
    }
    else if (info.layerDevice->colorSpace()->colorDepthId() == Float32BitsColorDepthID) {
        pixelType = Imf::FLOAT;
    }

    info.pixelType = pixelType;

    if (info.layerDevice->colorSpace()->colorModelId() == RGBAColorModelID) {
        header.channels().insert("R", Imf::Channel(pixelType));
        header.channels().insert("G", Imf::Channel(pixelType));
        header.channels().insert("B", Imf::Channel(pixelType));
        header.channels().insert("A", Imf::Channel(pixelType));

        info.channels.push_back("R");
        info.channels.push_back("G");
        info.channels.push_back("B");
        info.channels.push_back("A");
    } else if (info.layerDevice->colorSpace()->colorModelId() == GrayAColorModelID) {
        header.channels().insert("Y", Imf::Channel(pixelType));
        header.channels().insert("A", Imf::Channel(pixelType));

        info.channels.push_back("Y");
        info.channels.push_back("A");
    } else if (info.layerDevice->colorSpace()->colorModelId() == XYZAColorModelID) {
        header.channels().insert("X", Imf::Channel(pixelType));
        header.channels().insert("Y", Imf::Channel(pixelType));
        header.channels().insert("Z", Imf::Channel(pixelType));
        header.channels().insert("A", Imf::Channel(pixelType));

        info.channels.push_back("X");
        info.channels.push_back("Y");
        info.channels.push_back("Z");
        info.channels.push_back("A");
    }

    // Open file for writing
    try {
        Imf::OutputFile file(filename.PkToUtf8().c_str(), header);

        PkList<ExrPaintLayerSaveInfo> informationObjects;
        informationObjects.push_back(info);
        encodeData(file, informationObjects, width, height);
        return ImportExportCodes::OK;

    } catch(std::exception &e) {
        dbgFile << "Exception while writing to exr file: " << e.what();
        if (!KisImportExportAdditionalChecks::isFileWritable(filename)) {
            return ImportExportCodes::NoAccessToWrite;
        }
        return ImportExportCodes::ErrorWhileWriting;
    }

}

PkString remap(const PkMap<PkString, PkString>& current2original, const PkString& current)
{
    if (current2original.contains(current)) {
        return current2original[current];
    }
    return current;
}

void EXRConverter::Private::makeLayerNamesUnique(PkList<ExrPaintLayerSaveInfo>& informationObjects)
{
    using InfoIterator = PkList<ExrPaintLayerSaveInfo>::iterator;
    std::map<PkString, std::vector<InfoIterator>> namesMap;

    {
        PkList<ExrPaintLayerSaveInfo>::iterator it = informationObjects.begin();
        PkList<ExrPaintLayerSaveInfo>::iterator end = informationObjects.end();

        for (; it != end; ++it) {
            namesMap[it->name].push_back(it);
        }
    }

    for (auto &entry : namesMap) {
        const PkString &key = entry.first;
        if (entry.second.size() > 1) {
            KIS_ASSERT_RECOVER(key.right(1) == ".") { continue; }
            PkString strippedName = key.left(key.size() - 1); // trim the ending dot
            int nameCounter = 0;

            for (InfoIterator info : entry.second) {
                PkString newName =
                        PkString("%1_%2.")
                        .arg(strippedName)
                        .arg(nameCounter++);

                info->name = newName;

                PkList<PkString>::iterator channelsIt = info->channels.begin();
                PkList<PkString>::iterator channelsEnd = info->channels.end();

                for  (; channelsIt != channelsEnd; ++channelsIt) {
                    if (channelsIt->startsWith(key)) {
                        *channelsIt = newName + channelsIt->mid(key.size());
                    }
                }
            }
        }
    }

}

void EXRConverter::Private::recBuildPaintLayerSaveInfo(PkList<ExrPaintLayerSaveInfo>& informationObjects, const PkString& name, KisGroupLayerSP parent)
{
    PkSet<KisNodeSP> layersNotSaved;

    for (uint i = 0; i < parent->childCount(); ++i) {
        KisNodeSP node = parent->at(i);

        if (KisPaintLayerSP paintLayer = dynamic_cast<KisPaintLayer*>(node.data())) {
            PkMap<PkString, PkString> current2original;

            if (paintLayer->metaData()->containsEntry(KisMetaData::SchemaRegistry::instance()->create("http://krita.org/exrchannels/1.0/" , "exrchannels"), "channelsmap")) {

                const KisMetaData::Entry& entry = paintLayer->metaData()->getEntry(KisMetaData::SchemaRegistry::instance()->create("http://krita.org/exrchannels/1.0/" , "exrchannels"), "channelsmap");
                PkList< KisMetaData::Value> values = entry.value().asArray();

                for (const KisMetaData::Value& value : values) {
                    PkMap<PkString, KisMetaData::Value> map = value.asStructure();
                    if (map.contains("original") && map.contains("current")) {
                        current2original[map["current"].toString()] = map["original"].toString();
                    }
                }

            }

            ExrPaintLayerSaveInfo info;
            info.name = name + paintLayer->name() + PkString(".");
            info.layer = paintLayer;
            info.layerDevice = wrapLayerDevice(paintLayer->paintDevice());

            if (info.name == PkString(HDR_LAYER) + ".") {
                info.channels.push_back("R");
                info.channels.push_back("G");
                info.channels.push_back("B");
                info.channels.push_back("A");
            }
            else {

                if (info.layerDevice->colorSpace()->colorModelId() == RGBAColorModelID) {
                    info.channels.push_back(info.name + remap(current2original, "R"));
                    info.channels.push_back(info.name + remap(current2original, "G"));
                    info.channels.push_back(info.name + remap(current2original, "B"));
                    info.channels.push_back(info.name + remap(current2original, "A"));
                }
                else if (info.layerDevice->colorSpace()->colorModelId() == GrayAColorModelID) {
                    info.channels.push_back(info.name + remap(current2original, "Y"));
                    info.channels.push_back(info.name + remap(current2original, "A"));
                } else if (info.layerDevice->colorSpace()->colorModelId() == XYZAColorModelID) {
                    info.channels.push_back(info.name + remap(current2original, "X"));
                    info.channels.push_back(info.name + remap(current2original, "Y"));
                    info.channels.push_back(info.name + remap(current2original, "Z"));
                    info.channels.push_back(info.name + remap(current2original, "A"));
                }
            }

            if (info.layerDevice->colorSpace()->colorDepthId() == Float16BitsColorDepthID) {
                info.pixelType = Imf::HALF;
            }
            else if (info.layerDevice->colorSpace()->colorDepthId() == Float32BitsColorDepthID) {
                info.pixelType = Imf::FLOAT;
            }
            else {
                info.pixelType = Imf::NUM_PIXELTYPES;
            }

            if (info.pixelType < Imf::NUM_PIXELTYPES) {
                dbgFile << "Going to save layer" << info.name;
                informationObjects.push_back(info);
            }
            else {
                warnFile << "Will not save layer" << info.name;
                layersNotSaved.insert(node);
            }

        }
        else if (KisGroupLayerSP groupLayer = dynamic_cast<KisGroupLayer*>(node.data())) {
            recBuildPaintLayerSaveInfo(informationObjects,
                                       name + groupLayer->name() + PkString("."), groupLayer);
        }
        else {
            /**
             * The EXR can store paint and group layers only. The rest will
             * go to /dev/null :(
             */
            layersNotSaved.insert(node);
        }
    }

    if (!layersNotSaved.isEmpty()) {
        reportLayersNotSaved(layersNotSaved);
    }
}

void EXRConverter::Private::reportLayersNotSaved(const PkSet<KisNodeSP> &layersNotSaved)
{
    PkString layersList;
    for (KisNodeSP node : layersNotSaved) {
        layersList += PkString("<li>%1 (unsupported node type)</li>").arg(node->name());
    }

    PkString msg =
        PkString("<p>The following layers have a type that is not supported by EXR format:</p>"
                 "<r><ul>%1</ul></p>"
                 "<p><warning>these layers have <b>not</b> been saved to the final EXR file</warning></p>")
            .arg(layersList);

    errorMessage = msg;
}

PkString EXRConverter::Private::fetchExtraLayersInfo(PkList<ExrPaintLayerSaveInfo>& informationObjects)
{
    KIS_ASSERT_RECOVER_NOOP(!informationObjects.isEmpty());

    if (informationObjects.size() == 1 && informationObjects[0].name == PkString(HDR_LAYER) + ".") {
        return PkString();
    }

    PkXmlDocument doc("krita-extra-layers-info");
    doc.appendChild(doc.createElement("root"));
    PkXmlElement rootElement = doc.documentElement();

    for (int i = 0; i < informationObjects.size(); i++) {
        ExrPaintLayerSaveInfo &info = informationObjects[i];
        quint32 unused;
        KisSaveXmlVisitor visitor(doc, rootElement, unused, PkString(), false);

        // EXR data is saved without the offset, saving code uses normal iterators
        // that don't know about the layer offset, hence passing `false` for saving
        // the offsets
        PkXmlElement el = visitor.savePaintLayerAttributes(info.layer.data(), doc, false);

        // cut the ending '.'
        PkString strippedName = info.name.left(info.name.size() - 1);

        el.setAttribute(EXR_NAME, strippedName);

        rootElement.appendChild(el);
    }

    return doc.toString();
}

KisImportExportErrorCode EXRConverter::buildFile(const PkString &filename, KisGroupLayerSP layer, bool flatten)
{
    KIS_ASSERT_RECOVER_RETURN_VALUE(layer, ImportExportCodes::InternalError);

    KisImageSP image = layer->image();
    KIS_ASSERT_RECOVER_RETURN_VALUE(image, ImportExportCodes::InternalError);

    qint32 height = image->height();
    qint32 width = image->width();
    Imf::Header header(width, height);

    if (flatten) {
        KisPaintDeviceSP pd = new KisPaintDevice(*image->projection());
        KisPaintLayerSP l = new KisPaintLayer(image, "projection", OPACITY_OPAQUE_U8, pd);
        return buildFile(filename, l);
    }
    else {
        PkList<ExrPaintLayerSaveInfo> informationObjects;
        d->recBuildPaintLayerSaveInfo(informationObjects, "", layer);

        if(informationObjects.isEmpty()) {
            return ImportExportCodes::FormatColorSpaceUnsupported;
        }
        d->makeLayerNamesUnique(informationObjects);

        const std::string extraLayersInfo = d->fetchExtraLayersInfo(informationObjects).PkToUtf8();
        if (!extraLayersInfo.empty()) {
            header.insert(EXR_KRITA_LAYERS, Imf::StringAttribute(extraLayersInfo));
        }
        dbgFile << informationObjects.size() << " layers to save";
        for (const ExrPaintLayerSaveInfo& info : informationObjects) {
            if (info.pixelType < Imf::NUM_PIXELTYPES) {
                for (const PkString& channel : info.channels) {
                    dbgFile << channel << " " << info.pixelType;
                    header.channels().insert(channel.PkToUtf8().c_str(), Imf::Channel(info.pixelType));
                }
            }
        }

        // Open file for writing
        try {
            Imf::OutputFile file(filename.PkToUtf8().c_str(), header);
            encodeData(file, informationObjects, width, height);
            return ImportExportCodes::OK;
        } catch(std::exception &e) {
            dbgFile << "Exception while writing to exr file: " << e.what();
            if (!KisImportExportAdditionalChecks::isFileWritable(filename)) {
                return ImportExportCodes::NoAccessToWrite;
            }
            return ImportExportCodes::ErrorWhileWriting;
        }

    }
}
