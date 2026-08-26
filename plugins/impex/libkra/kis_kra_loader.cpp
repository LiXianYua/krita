/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <klocalizedstring.h>
#include "kis_kra_loader.h"

#include <PkStringList.h>

#include <PkMemoryStream.h>
#include <PkVersionNumber.h>
#include <PkNodeId.h>

#include <filesystem>

#include <KoStore.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorSpaceEngine.h>
#include <KoColorProfile.h>
#include <KoDocumentInfo.h>
#include <KisImportExportManager.h>
#include <KisImportUserFeedbackInterface.h>
#include <KoStoreDevice.h>
#include <KoResourceServer.h>
#include <KisResourceStorage.h>
#include <KisGlobalResourcesInterface.h>
#include <KisResourceModel.h>

#include <filter/kis_filter.h>
#include <filter/kis_filter_registry.h>
#include <generator/kis_generator.h>
#include <generator/kis_generator_layer.h>
#include <generator/kis_generator_registry.h>
#include <kis_adjustment_layer.h>
#include <kis_annotation.h>
#include <kis_base_node.h>
#include <kis_clone_layer.h>
#include <kis_debug.h>
#include <kis_assert.h>
#include <kis_external_layer_iface.h>
#include <kis_filter_mask.h>
#include <kis_transform_mask.h>
#include "lazybrush/kis_colorize_mask.h"
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_name_server.h>
#include <kis_paint_layer.h>
#include <kis_selection.h>
#include <kis_selection_mask.h>
#include <kis_shape_layer.h>
#include <kis_transparency_mask.h>
#include <kis_layer_composition.h>
#include <kis_file_layer.h>
#include <kis_psd_layer_style.h>
#include <kis_asl_layer_style_serializer.h>
#include "kis_keyframe_channel.h"
#include <kis_filter_configuration.h>
#include "KisReferenceImagesLayer.h"
#include "KisReferenceImage.h"
#include <KoColorSet.h>

#include "KisDocument.h"
#include "kis_kra_tags.h"
#include "kis_kra_utils.h"
#include "kis_kra_load_visitor.h"
#include "kis_dom_utils.h"
#include "kis_image_animation_interface.h"
#include "kis_time_span.h"
#include "kis_grid_config.h"
#include "kis_guides_config.h"
#include "kis_image_config.h"
#include "KisProofingConfiguration.h"
#include "kis_layer_properties_icons.h"
#include "KisMirrorAxisConfig.h"

/*
  Color model id comparison through the ages:

2.4        2.5          2.6         ideal

ALPHA      ALPHA        ALPHA       ALPHAU8

CMYK       CMYK         CMYK        CMYKAU8
           CMYKAF32     CMYKAF32
CMYKA16    CMYKAU16     CMYKAU16

GRAYA      GRAYA        GRAYA       GRAYAU8
GrayF32    GRAYAF32     GRAYAF32
GRAYA16    GRAYAU16     GRAYAU16

LABA       LABA         LABA        LABAU16
           LABAF32      LABAF32
           LABAU8       LABAU8

RGBA       RGBA         RGBA        RGBAU8
RGBA16     RGBA16       RGBA16      RGBAU16
RgbAF32    RGBAF32      RGBAF32
RgbAF16    RgbAF16      RGBAF16

XYZA16     XYZA16       XYZA16      XYZAU16
           XYZA8        XYZA8       XYZAU8
XyzAF16    XyzAF16      XYZAF16
XyzAF32    XYZAF32      XYZAF32

YCbCrA     YCBCRA8      YCBCRA8     YCBCRAU8
YCbCrAU16  YCBCRAU16    YCBCRAU16
           YCBCRF32     YCBCRF32
 */

using namespace KRA;

struct KisKraLoader::Private
{
public:
    KisDocument* document;
    PkString imageName; // used to be stored in the image, is now in the documentInfo block
    PkString imageComment; // used to be stored in the image, is now in the documentInfo block
    PkMap<KisNode*, PkString> layerFilenames; // temp storage during loading
    int syntaxVersion; // version of the fileformat we are loading
    PkVersionNumber kritaVersion;
    KisImportUserFeedbackInterface *feedbackInterface {nullptr};
    vKisNodeSP selectedNodes; // the nodes that were active when saving the document.
    PkMap<PkString, PkString> assistantsFilenames;
    StoryboardItemList storyboardItemList;
    StoryboardCommentList storyboardCommentList;
    PkList<KisPaintingAssistantSP> assistants;
    PkMap<KisNode*, PkString> keyframeFilenames;
    PkVector<PkString> paletteFilenames;
    PkVector<KoResourceSignature> resources;
    PkStringList errorMessages;
    PkStringList warningMessages;
    PkList<KisAnnotationSP> annotations;
};

void convertColorSpaceNames(PkString &colorspacename, PkString &profileProductName) {
    if (colorspacename  == "Grayscale + Alpha") {
        colorspacename  = "GRAYA";
        profileProductName = PkString();
    }
    else if (colorspacename == "RgbAF32") {
        colorspacename = "RGBAF32";
        profileProductName = PkString();
    }
    else if (colorspacename == "RgbAF16") {
        colorspacename = "RGBAF16";
        profileProductName = PkString();
    }
    else if (colorspacename == "CMYKA16") {
        colorspacename = "CMYKAU16";
    }
    else if (colorspacename == "GrayF32") {
        colorspacename =  "GRAYAF32";
        profileProductName = PkString();
    }
    else if (colorspacename == "GRAYA16") {
        colorspacename  = "GRAYAU16";
    }
    else if (colorspacename == "XyzAF16") {
        colorspacename  = "XYZAF16";
        profileProductName = PkString();
    }
    else if (colorspacename == "XyzAF32") {
        colorspacename  = "XYZAF32";
        profileProductName = PkString();
    }
    else if (colorspacename == "YCbCrA") {
        colorspacename  = "YCBCRA8";
    }
    else if (colorspacename == "YCbCrAU16") {
        colorspacename  = "YCBCRAU16";
    }
}

KisKraLoader::KisKraLoader(KisDocument * document, int syntaxVersion, const PkVersionNumber &kritaVersion, KisImportUserFeedbackInterface *feedbackInterface)
    : m_d(new Private())
{
    m_d->document = document;
    m_d->syntaxVersion = syntaxVersion;
    m_d->kritaVersion = kritaVersion;
    m_d->feedbackInterface = feedbackInterface;
}


KisKraLoader::~KisKraLoader()
{
    delete m_d;
}


KisImageSP KisKraLoader::loadXML(const PkXmlElement& imageElement)
{
    PkString attr;
    KisImageSP image = 0;
    qint32 width;
    qint32 height;
    PkString profileProductName;
    double xres;
    double yres;
    PkString colorspacename;
    const KoColorSpace * cs;

    if ((attr = imageElement.attribute(MIME)) == NATIVE_MIMETYPE) {

        if ((m_d->imageName = imageElement.attribute(NAME)).isEmpty()) {
            m_d->errorMessages << i18n("Image does not have a name.");
            return KisImageSP(0);
        }

        if ((attr = imageElement.attribute(WIDTH)).isEmpty()) {
            m_d->errorMessages << i18n("Image does not specify a width.");
            return KisImageSP(0);
        }
        width = KisDomUtils::toInt(attr);

        if ((attr = imageElement.attribute(HEIGHT)).isEmpty()) {
            m_d->errorMessages << i18n("Image does not specify a height.");
            return KisImageSP(0);
        }

        height = KisDomUtils::toInt(attr);

        m_d->imageComment = imageElement.attribute(DESCRIPTION);

        xres = 100.0 / 72.0;
        if (!(attr = imageElement.attribute(X_RESOLUTION)).isEmpty()) {
            qreal value = KisDomUtils::toDouble(attr);

            if (value > 0) {
                xres = value / 72.0;
            }
        }

        yres = 100.0 / 72.0;
        if (!(attr = imageElement.attribute(Y_RESOLUTION)).isEmpty()) {
            qreal value = KisDomUtils::toDouble(attr);
            if (value > 0) {
                yres = value / 72.0;
            }
        }

        if ((colorspacename = imageElement.attribute(COLORSPACE_NAME)).isEmpty()) {
            // An old file: take a reasonable default.
            // Krita didn't support anything else in those
            // days anyway.
            colorspacename = "RGBA";
        }

        profileProductName = imageElement.attribute(PROFILE);
        // A hack for an old colorspacename
        convertColorSpaceNames(colorspacename, profileProductName);

        PkString colorspaceModel = KoColorSpaceRegistry::instance()->colorSpaceColorModelId(colorspacename).id();
        PkString colorspaceDepth = KoColorSpaceRegistry::instance()->colorSpaceColorDepthId(colorspacename).id();

        if (profileProductName.isEmpty()) {
            // no mention of profile so get default profile";
            cs = KoColorSpaceRegistry::instance()->colorSpace(colorspaceModel, colorspaceDepth, "");
        } else {
            cs = KoColorSpaceRegistry::instance()->colorSpace(colorspaceModel, colorspaceDepth, profileProductName);
        }

        if (cs == 0) {
            // try once more without the profile
            cs = KoColorSpaceRegistry::instance()->colorSpace(colorspaceModel, colorspaceDepth, "");
            if (cs == 0) {
                m_d->errorMessages << i18n("Image specifies an unsupported color model: %1.", colorspacename);
                return KisImageSP(0);
            }
        }
        KisProofingConfigurationSP proofingConfig;
        if (!(attr = imageElement.attribute(PROOFINGPROFILENAME)).isEmpty()) {
            // initialize config only if ptofile name is present
            proofingConfig = KisImageConfig(true).defaultProofingconfiguration();
            proofingConfig->proofingProfile = attr;
        }
        if (proofingConfig && !(attr = imageElement.attribute(PROOFINGMODEL)).isEmpty()) {
            proofingConfig->proofingModel = attr;
        }
        if (proofingConfig && !(attr = imageElement.attribute(PROOFINGDEPTH)).isEmpty()) {
            proofingConfig->proofingDepth = attr;
        }
        if (proofingConfig && !(attr = imageElement.attribute(PROOFINGINTENT)).isEmpty()) {
            proofingConfig->conversionIntent = (KoColorConversionTransformation::Intent) KisDomUtils::toInt(attr);
        }
        if (proofingConfig && !(attr = imageElement.attribute(PROOFINGDISPLAYINTENT)).isEmpty()) {
            proofingConfig->displayIntent = (KoColorConversionTransformation::Intent) KisDomUtils::toInt(attr);
        }
        if (proofingConfig && !(attr = imageElement.attribute(PROOFINGDISPLAYMODE)).isEmpty()) {
            if (attr == "monitor") {
                proofingConfig->displayMode = KisProofingConfiguration::Monitor;
            } else if (attr == "paper") {
                proofingConfig->displayMode = KisProofingConfiguration::Paper;
            } else {
                proofingConfig->displayMode = KisProofingConfiguration::Custom;
            }
        }
        if (proofingConfig && !(attr = imageElement.attribute(PROOFINGBLACKPOINTCOMPENSATION)).isEmpty()) {
            proofingConfig->useBlackPointCompensationFirstTransform = (attr == "true");
        }

        if (proofingConfig && !(attr = imageElement.attribute(PROOFINGDISPLAYBLACKPOINTCOMPENSATION)).isEmpty()) {
            proofingConfig->displayFlags.setFlag(KoColorConversionTransformation::BlackpointCompensation, attr == "true");
        }

        if (proofingConfig && !(attr = imageElement.attribute(PROOFINGADAPTATIONSTATE)).isEmpty()) {
            const qreal legacyAdaptationState = KisDomUtils::toDouble(attr);
            proofingConfig->setLegacyAdaptationState(legacyAdaptationState);
        }

        if (m_d->document) {
            image = new KisImage(m_d->document->createUndoStore(), width, height, cs, m_d->imageName);
        }
        else {
            image = new KisImage(0, width, height, cs, m_d->imageName);
        }
        image->setResolution(xres, yres);
        loadNodes(imageElement, image, const_cast<KisGroupLayer*>(image->rootLayer().data()));


        PkXmlNode child;
        for (child = imageElement.lastChild(); !child.isNull(); child = child.previousSibling()) {
            PkXmlElement e = child.toElement();

            if(e.tagName() == CANVASPROJECTIONCOLOR) {
                if (e.hasAttribute(COLORBYTEDATA)) {
                    const PkByteArray colorData = KRA::base64Decode(e.attribute(COLORBYTEDATA).PkToUtf8());
                    KoColor color((const quint8*)colorData.data(), image->colorSpace());
                    image->setDefaultProjectionColor(color);
                }
            }

            if(e.tagName() == COLORHISTORY) {
                PkList<KoColor> colors = loadKoColors(e);
                m_d->document->setColorHistory(colors);
            }

            if(e.tagName() == GLOBALASSISTANTSCOLOR) {
                if (e.hasAttribute(SIMPLECOLORDATA)) {
                    PkString colorData = e.attribute(SIMPLECOLORDATA);
                    m_d->document->setAssistantsGlobalColor(KisDomUtils::qStringToQColor(colorData));
                }
            }

            if (proofingConfig && e.tagName()== PROOFINGWARNINGCOLOR) {
                PkXmlDocument dom;
                PkXmlNode node = e;
                dom.appendChild(dom.importNode(node, true));
                PkXmlElement eq = dom.firstChildElement();
                proofingConfig->warningColor = KoColor::fromXML(eq.firstChildElement(), Integer8BitsColorDepthID.id());
            }

            // COMPATIBILITY -- Load Animation Metadata from OLD KRA files.
            if (e.tagName().toLower() == "animation") {
                loadAnimationMetadataFromXML(e, image);
            }
        }

        if (proofingConfig) {
            image->setProofingConfiguration(proofingConfig);
        }

        for (child = imageElement.lastChild(); !child.isNull(); child = child.previousSibling()) {
            PkXmlElement e = child.toElement();
            if (e.tagName() == "compositions") {
                loadCompositions(e, image);
            }
        }
    }

    PkXmlNode child;
    for (child = imageElement.lastChild(); !child.isNull(); child = child.previousSibling()) {
        PkXmlElement e = child.toElement();
        if (e.tagName() == "grid") {
            loadGrid(e);
        } else if (e.tagName() == "guides") {
            loadGuides(e);
        } else if (e.tagName() == MIRROR_AXIS) {
            loadMirrorAxis(e);
        } else if (e.tagName() == "assistants") {
            loadAssistantsList(e);
        } else if (e.tagName() == "audio") {
            backCompat_loadAudio(e, m_d->document);
        }
    }

    // reading palettes from XML
    for (child = imageElement.lastChild(); !child.isNull(); child = child.previousSibling()) {
        PkXmlElement e = child.toElement();
        if (e.tagName() == PALETTES) {
            for (PkXmlElement paletteElement = e.lastChildElement(); !paletteElement.isNull();
                 paletteElement = paletteElement.previousSiblingElement()) {
                PkString paletteName = paletteElement.attribute("filename");
                m_d->paletteFilenames.append(paletteName);
            }
            break;
        }
    }

    // reading resources from XML
    for (child = imageElement.lastChild(); !child.isNull(); child = child.previousSibling()) {
        PkXmlElement e = child.toElement();
        if (e.tagName() == RESOURCES) {
            for (PkXmlElement resourceElement = e.lastChildElement();
                 !resourceElement.isNull();
                 resourceElement = resourceElement.previousSiblingElement())
            {
                KoResourceSignature resourceItem;
                resourceItem.filename = resourceElement.attribute("filename");
                resourceItem.md5sum = resourceElement.attribute("md5sum");
                resourceItem.type = resourceElement.attribute("type");
                resourceItem.name = resourceElement.attribute("name");
                m_d->resources.append(resourceItem);
            }
            break;
        }
    }

    // reading the extra annotations from XML
    for (child = imageElement.lastChild(); !child.isNull(); child = child.previousSibling()) {
        PkXmlElement e = child.toElement();
        if (e.tagName() == ANNOTATIONS) {
            for (PkXmlElement annotationElement = e.firstChildElement();
                 !annotationElement.isNull();
                 annotationElement = annotationElement.nextSiblingElement())
            {
                PkString type = annotationElement.attribute("type");
                PkString description = annotationElement.attribute("description");

                KisAnnotationSP annotation = new KisAnnotation(type, description, PkByteArray());
                m_d->annotations << annotation;
            }
            break;
        }
    }

    return image;
}

void KisKraLoader::loadBinaryData(KoStore * store, KisImageSP image, const PkString & uri, bool external)
{
    // icc profile: if present, this overrides the profile product name loaded in loadXML.
    PkString location = external ? PkString() : uri;
    location += m_d->imageName + ICC_PATH;
    if (store->hasFile(location)) {
        if (store->open(location)) {
            PkByteArray data; data.resize(store->size());
            bool res = (store->read(data.data(), store->size()) > -1);
            store->close();
            if (res) {
                PkString colorspaceModel = image->colorSpace()->colorModelId().id();
                PkString colorspaceDepth = image->colorSpace()->colorDepthId().id();
                const KoColorProfile *profile = KoColorSpaceRegistry::instance()->createColorProfile(colorspaceModel, image->colorSpace()->colorDepthId().id(), data);
                if (profile && profile->valid()) {
                    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->colorSpace(colorspaceModel, colorspaceDepth, profile);
                    image->convertImageProjectionColorSpace(colorSpace);
                }
            }
        }
    }
    //load the embed proofing profile, it only needs to be loaded into Krita, not assigned.
    location = external ? PkString() : uri;
    location += m_d->imageName + ICC_PROOFING_PATH;
    if (store->hasFile(location)) {
        if (store->open(location)) {
            PkByteArray proofingData;
            proofingData.resize(store->size());
            bool proofingProfileRes = (store->read(proofingData.data(), store->size())>-1);
            store->close();

            KisProofingConfigurationSP proofingConfig = image->proofingConfiguration();
            if (!proofingConfig) {
                proofingConfig = KisImageConfig(true).defaultProofingconfiguration();
            }

            if (proofingProfileRes) {
                const KoColorProfile *proofingProfile = KoColorSpaceRegistry::instance()->createColorProfile(proofingConfig->proofingModel, proofingConfig->proofingDepth, proofingData);
                if (proofingProfile->valid()){
                    KoColorSpaceRegistry::instance()->addProfile(proofingProfile);
                }
            }
        }
    }


    // Load the layers data: if there is a profile associated with a layer it will be set now.
    KisKraLoadVisitor visitor(image, store, m_d->document->shapeController(), m_d->layerFilenames, m_d->keyframeFilenames, m_d->imageName, m_d->syntaxVersion, m_d->feedbackInterface);

    if (external) {
        visitor.setExternalUri(uri);
    }

    image->rootLayer()->accept(visitor);
    if (!visitor.errorMessages().isEmpty()) {
        m_d->errorMessages.append(visitor.errorMessages());
    }
    if (!visitor.warningMessages().isEmpty()) {
        m_d->warningMessages.append(visitor.warningMessages());
    }

    // annotations
    // exif
    location = external ? PkString() : uri;
    location += m_d->imageName + EXIF_PATH;
    if (store->hasFile(location)) {
        PkByteArray data;
        store->open(location);
        data = store->read(store->size());
        store->close();
        image->addAnnotation(KisAnnotationSP(new KisAnnotation("exif", "", data)));
    }


    // layer styles
    location = external ? PkString() : uri;
    location += m_d->imageName + LAYER_STYLES_PATH;
    if (store->hasFile(location)) {

        KisAslLayerStyleSerializer serializer;
        store->open(location);
        {
            KoStoreDevice device(store);
            device.open(PkStream::ReadOnly);

            /**
             * ASL loading code cannot work with non-sequential IO devices,
             * so convert the device beforehand!
             */
            PkByteArray buf = device.readAll();
            // PkMemoryStream 无内存缓冲(字节数组*) 对应的构造（libs/store 锁内，
            // S-05-b 不改）。等价语义：WriteOnly 写入 + seek(0) 回位 + 切 ReadOnly。
            PkMemoryStream raDevice;
            raDevice.open(PkStream::WriteOnly);
            raDevice.write(buf.data(), buf.size());
            raDevice.seek(0);
            raDevice.open(PkStream::ReadOnly);
            serializer.readFromDevice(raDevice);
        }
        store->close();

        if (serializer.isValid()) {
            const PkString resourceLocation = m_d->document->embeddedResourcesStorageId();
            serializer.assignAllLayerStylesToLayers(image->root(), resourceLocation);

        } else {
            warnKrita << "WARNING: Couldn't load layer styles library from .kra!";
        }
    }

    if (m_d->document && m_d->document->documentInfo()->aboutInfo("title").isEmpty())
        m_d->document->documentInfo()->setAboutInfo("title", m_d->imageName);
    if (m_d->document && m_d->document->documentInfo()->aboutInfo("comment").isEmpty())
        m_d->document->documentInfo()->setAboutInfo("comment", m_d->imageComment);

    loadAssistants(store, uri, external);

    // Annotations
    for (KisAnnotationSP annotation : m_d->annotations) {
        PkByteArray ba;
        location = external ? PkString() : uri;
        location += m_d->imageName + ANNOTATIONS_PATH + annotation->type();
        if (store->hasFile(location)) {
            store->open(location);
            KoStoreDevice device(store);
            device.open(PkStream::ReadOnly);
            ba = device.readAll();
            device.close();
            store->close();
            annotation->setAnnotation(ba);
            m_d->document->image()->addAnnotation(annotation);
        }
    }

}

void KisKraLoader::loadResources(KoStore *store, KisDocument *doc)
{
    PkList<KoColorSetSP> list;
    for (const PkString &filename : m_d->paletteFilenames) {
        KoColorSetSP newPalette(new KoColorSet(filename));
        store->open(m_d->imageName + PALETTE_PATH + filename);

        PkByteArray data = store->read(store->size());
        if (data.size() > 0) {
            newPalette->fromByteArray(data, KisGlobalResourcesInterface::instance());
            store->close();
            list.append(newPalette);
        } else {
            m_d->warningMessages.append(i18nc("Warning message on loading a .kra file", "Embedded palette is empty and cannot be loaded. The name of the palette: %1", filename));
        }
    }
    doc->setPaletteList(list);

    for (const KoResourceSignature &resourceItem : m_d->resources) {
        KisResourceModel model(resourceItem.type);
        if (model.resourcesForMD5(resourceItem.md5sum).isEmpty()) {
            store->open(RESOURCE_PATH + "/" + resourceItem.type + "/" + resourceItem.filename);

            if (!store->isOpen()) {
                m_d->warningMessages.append(i18nc("Warning message on loading a .kra file", "Embedded resource cannot be read. The filename of the resource: %1", resourceItem.filename));
                continue;
            }

            /// don't try to load the resource if its file is empty
            /// (which is a sign of a failed save operation)
            if (!store->device()->atEnd() && !doc->linkedResourcesStorageId().isEmpty()) {
                bool result = bool(model.importResource(resourceItem.filename, store->device(), false, doc->linkedResourcesStorageId()));
                if (!result) {
                    m_d->warningMessages.append(i18nc("Warning message on loading a .kra file", "Embedded resource cannot be imported. The filename of the resource: %1", resourceItem.filename));
                }
            }

            store->close();
        }
    }
}

void KisKraLoader::loadStoryboards(KoStore *store, KisDocument */*doc*/)
{
    if (!store->hasFile(m_d->imageName + STORYBOARD_PATH + "index.xml")) return;

    if (store->open(m_d->imageName + STORYBOARD_PATH + "index.xml")) {
        PkByteArray data = store->read(store->size());
        PkXmlDocument document;
        document.setContent(data);
        store->close();

        PkXmlElement root = document.documentElement();
        PkXmlNode node;
        for (node = root.lastChild(); !node.isNull(); node = node.previousSibling()) {
            if (node.isElement()) {
                PkXmlElement element = node.toElement();
                if (element.tagName() == "StoryboardItemList") {
                    loadStoryboardItemList(element);
                } else if (element.tagName() == "StoryboardCommentList") {
                    loadStoryboardCommentList(element);
                }
            }
        }
    }
}

void KisKraLoader::loadAnimationMetadata(KoStore *store, KisImageSP image)
{
    if (!store->hasFile(m_d->imageName + ANIMATION_METADATA_PATH + "index.xml")) return;

    if (store->open(m_d->imageName + ANIMATION_METADATA_PATH + "index.xml")) {
        PkByteArray data = store->read(store->size());
        PkXmlDocument document;
        document.setContent(data);
        store->close();

        PkXmlElement root = document.documentElement();
        loadAnimationMetadataFromXML(root, image);
    }
}

void KisKraLoader::loadAudio(KoStore *store, KisDocument *kisDoc)
{
    if (!store->hasFile(m_d->imageName + AUDIO_PATH + "index.xml")) return;

    if (store->open(m_d->imageName + AUDIO_PATH + "index.xml")) {
        PkByteArray byteData = store->read(store->size());
        PkXmlDocument xmlDocument;
        xmlDocument.setContent(byteData);
        store->close();

        PkXmlElement root = xmlDocument.documentElement();
        loadAudioXML(xmlDocument, root, kisDoc);
    }
}

void KisKraLoader::backCompat_loadAudio(const PkXmlElement& elem, KisDocument *document)
{
    PkXmlDocument dom;
    dom.appendChild(dom.importNode(elem, true));
    PkXmlElement qElement = dom.firstChildElement();

    PkString fileName;
    if (KisDomUtils::loadValue(qElement, "masterChannelPath", &fileName)) {
        const std::filesystem::path baseDirectory =
            std::filesystem::path(m_d->document->localFilePath().PkToUtf8()).parent_path();
        std::filesystem::path filePath =
            (baseDirectory / std::filesystem::path(fileName.PkToUtf8())).lexically_normal();

        if (!std::filesystem::exists(filePath)) {
            if (m_d->feedbackInterface) {
                PkString chosenUrl;
                m_d->feedbackInterface->askUser([&](PkWidget *parent) {
                    (void)i18nc(
                                "@info",
                                "Audio channel file \"%1\" doesn't exist!\n\n"
                                "Expected path:\n"
                                "%2\n\n"
                                "Do you want to locate it manually?", fileName, filePath.string().c_str());

                    chosenUrl = KisImportExportManager::askForAudioFileName(filePath.parent_path().string().c_str(), parent);
                    return !chosenUrl.isEmpty();
                });

                if (!chosenUrl.isEmpty()) {
                    filePath = std::filesystem::path(chosenUrl.PkToUtf8());
                }
            }
        }

        if (std::filesystem::exists(filePath)) {
            PkVector<std::filesystem::path> clipFiles;

            clipFiles.append(filePath);

            document->setAudioTracks(clipFiles);
        }
    }

    // Note: Muting has been removed from backCompat due to it no longer being document-specific.

    qreal audioVolume = 1.0;
    if (KisDomUtils::loadValue(qElement, "audioVolume", &audioVolume)) {
        document->setAudioVolume(audioVolume);
    }
}

vKisNodeSP KisKraLoader::selectedNodes() const
{
    return m_d->selectedNodes;
}

PkList<KisPaintingAssistantSP> KisKraLoader::assistants() const
{
    return m_d->assistants;
}

StoryboardItemList KisKraLoader::storyboardItemList() const
{
    return m_d->storyboardItemList;
}

StoryboardCommentList KisKraLoader::storyboardCommentList() const
{
    return m_d->storyboardCommentList;
}

PkStringList KisKraLoader::errorMessages() const
{
    return m_d->errorMessages;
}

PkStringList KisKraLoader::warningMessages() const
{
    return m_d->warningMessages;
}

PkString KisKraLoader::imageName() const
{
    return m_d->imageName;
}


void KisKraLoader::loadAssistants(KoStore *store, const PkString &uri, bool external)
{
    // 跨锁桩（S-09-f 恢复）：assistant 加载功能在 plugins/assistants 剥 Qt 完成后
    // 恢复。现在不读 .assistant 文件、不引用 KisPaintingAssistant 族。
    (void)store;
    (void)uri;
    (void)external;
}

void KisKraLoader::loadAnimationMetadataFromXML(const PkXmlElement &element, KisImageSP image)
{
    PkXmlDocument qDom;
    PkXmlNode node = element;
    qDom.appendChild(qDom.importNode(node, true));
    PkXmlElement rootElement = qDom.firstChildElement();

    float framerate;
    KisTimeSpan range;
    int currentTime;
    PkString string;

    KisImageAnimationInterface *animation = image->animationInterface();

    if (KisDomUtils::loadValue(rootElement, "framerate", &framerate)) {
        animation->setFramerate(framerate);
    }

    if (KisDomUtils::loadValue(rootElement, "range", &range)) {
        animation->setDocumentRange(range);
    }

    if (KisDomUtils::loadValue(rootElement, "currentTime", &currentTime)) {
        animation->switchCurrentTimeAsync(currentTime);
    }

    {
        int initialFrameNumber = -1;
        PkXmlElement exportElement = rootElement.firstChildElement("export-settings");
        if (!exportElement.isNull()) {
            if (KisDomUtils::loadValue(exportElement, "sequenceFilePath", &string)) {
                animation->setExportSequenceFilePath(string);
            }

            if (KisDomUtils::loadValue(exportElement, "sequenceBaseName", &string)) {
                animation->setExportSequenceBaseName(string);
            }

            if (KisDomUtils::loadValue(exportElement, "sequenceInitialFrameNumber", &initialFrameNumber)) {
                animation->setExportInitialFrameNumber(initialFrameNumber);
            }
        }
    }

    animation->setExportSequenceBaseName(string);
}

KisNodeSP KisKraLoader::loadNodes(const PkXmlElement& element, KisImageSP image, KisNodeSP parent)
{

    PkXmlNode node = element.firstChild();
    PkXmlNode child;

    if (!node.isNull()) {

        if (node.isElement()) {

            // See https://bugs.kde.org/show_bug.cgi?id=408963, where there is a selection mask that is a child of the
            // the projection. That needs to be treated as a global selection, so we keep track of those.
            vKisNodeSP topLevelSelectionMasks;
            if (node.nodeName().toUpper() == LAYERS.toUpper() || node.nodeName().toUpper() == MASKS.toUpper()) {
                for (child = node.lastChild(); !child.isNull(); child = child.previousSibling()) {
                    KisNodeSP node = loadNode(child.toElement(), image);

                    if (node && parent.data() == image->rootLayer().data() && node->inherits("KisSelectionMask") && image->rootLayer()->childCount() > 0) {
                        topLevelSelectionMasks << node;
                        continue;
                    }

                    if (node ) {
                        image->addNode(node, parent);
                        if (node->inherits("KisLayer") && child.childNodes().count() > 0) {
                            loadNodes(child.toElement(), image, node);
                        }
                    }
                }

                KisSelectionMaskSP activeSelectionMask;
                for (KisNodeSP node : topLevelSelectionMasks) {
                    KisSelectionMask *mask = dynamic_cast<KisSelectionMask*>(node.data());
                    if (mask->active()) {
                        if (activeSelectionMask) {
                            m_d->warningMessages << i18n("Two global selection masks in active state found. \"%1\" is kept active, \"%2\" is deactivated", activeSelectionMask->name(), mask->name());
                            mask->setActive(false);
                            KIS_ASSERT(!mask->active());
                        } else {
                            activeSelectionMask = mask;
                        }
                    }

                    image->addNode(mask, parent);
                }
            }
        }
    }

    return parent;
}

#include <KoColorSpaceBlendingPolicy.h>

KisNodeSP KisKraLoader::loadNode(const PkXmlElement& element, KisImageSP image)
{
    // Nota bene: If you add new properties to layers, you should
    // ALWAYS define a default value in case the property is not
    // present in the layer definition: this helps a LOT with backward
    // compatibility.
    PkString name = element.attribute(NAME, "No Name");

    PkNodeId id = PkNodeId(element.attribute(UUID, PkNodeId().toString()));

    qint32 x = element.attribute(X, "0").toInt();
    qint32 y = element.attribute(Y, "0").toInt();

    qint32 opacity = element.attribute(OPACITY, PkString().arg((int)(OPACITY_OPAQUE_U8))).toInt();
    if (opacity < OPACITY_TRANSPARENT_U8) opacity = OPACITY_TRANSPARENT_U8;
    if (opacity > OPACITY_OPAQUE_U8) opacity = OPACITY_OPAQUE_U8;

    const KoColorSpace* colorSpace = 0;
    if ((element.attribute(COLORSPACE_NAME)).isEmpty()) {
        dbgFile << "No attribute color space for layer: " << name;
        colorSpace = image->colorSpace();
    } else {
        PkString colorspacename = element.attribute(COLORSPACE_NAME);
        PkString profileProductName = element.attribute(PROFILE);

        convertColorSpaceNames(colorspacename, profileProductName);

        PkString colorspaceModel = KoColorSpaceRegistry::instance()->colorSpaceColorModelId(colorspacename).id();
        PkString colorspaceDepth = KoColorSpaceRegistry::instance()->colorSpaceColorDepthId(colorspacename).id();
        dbgFile << "Searching color space: " << colorspacename << colorspaceModel << colorspaceDepth << " for layer: " << name;
        // use default profile - it will be replaced later in completeLoading

        if (profileProductName.isEmpty()) {
            // no mention of profile so get default profile";
            colorSpace = KoColorSpaceRegistry::instance()->colorSpace(colorspaceModel, colorspaceDepth, "");
        } else {
            colorSpace = KoColorSpaceRegistry::instance()->colorSpace(colorspaceModel, colorspaceDepth, profileProductName);
        }

        dbgFile << "found colorspace" << colorSpace;
        if (!colorSpace) {
            m_d->warningMessages << i18n("Layer %1 specifies an unsupported color model: %2.", name, colorspacename);
            return 0;
        }
    }

    const bool visible = element.attribute(VISIBLE, "1") == "0" ? false : true;
    const bool locked = element.attribute(LOCKED, "0") == "0" ? false : true;
    const bool collapsed = element.attribute(COLLAPSED, "0") == "0" ? false : true;
    int colorLabelIndex = element.attribute(COLOR_LABEL, "0").toInt();
    constexpr int colorLabelCount = 9;
    if (colorLabelIndex >= colorLabelCount) {
        colorLabelIndex = colorLabelCount - 1;
    }

    // Now find out the layer type and do specific handling
    PkString nodeType;

    if (m_d->syntaxVersion == 1) {
        nodeType = element.attribute("layertype");
        if (nodeType.isEmpty()) {
            nodeType = PAINT_LAYER;
        }
    }
    else {
        nodeType = element.attribute(NODE_TYPE);
    }

    if (nodeType.isEmpty()) {
        m_d->warningMessages << i18n("Layer %1 has an unsupported type.", name);
        return 0;
    }



    KisNodeSP node = 0;

    if (nodeType == PAINT_LAYER)
        node = loadPaintLayer(element, image, name, colorSpace, opacity);
    else if (nodeType == GROUP_LAYER)
        node = loadGroupLayer(element, image, name, colorSpace, opacity);
    else if (nodeType == ADJUSTMENT_LAYER)
        node = loadAdjustmentLayer(element, image, name, colorSpace, opacity);
    else if (nodeType == SHAPE_LAYER)
        node = loadShapeLayer(element, image, name, colorSpace, opacity);
    else if (nodeType == GENERATOR_LAYER)
        node = loadGeneratorLayer(element, image, name, colorSpace, opacity);
    else if (nodeType == CLONE_LAYER)
        node = loadCloneLayer(element, image, name, colorSpace, opacity);
    else if (nodeType == FILTER_MASK)
        node = loadFilterMask(image, element);
    else if (nodeType == TRANSFORM_MASK)
        node = loadTransformMask(image, element);
    else if (nodeType == TRANSPARENCY_MASK)
        node = loadTransparencyMask(image, element);
    else if (nodeType == SELECTION_MASK)
        node = loadSelectionMask(image, element);
    else if (nodeType == COLORIZE_MASK)
        node = loadColorizeMask(image, element, colorSpace);
    else if (nodeType == FILE_LAYER)
        node = loadFileLayer(element, image, name, opacity, colorSpace);
    else if (nodeType == REFERENCE_IMAGES_LAYER)
        node = loadReferenceImagesLayer(element, image);
    else {
        m_d->warningMessages << i18n("Layer %1 has an unsupported type: %2.", name, nodeType);
        return 0;
    }

    // Loading the node went wrong. Return empty node and leave to
    // upstream to complain to the user
    if (!node) {
        m_d->warningMessages << i18n("Failure loading layer %1 of type: %2.", name, nodeType);
        return 0;
    }

    node->setVisible(visible, true);
    node->setUserLocked(locked);
    node->setCollapsed(collapsed);
    node->setColorLabelIndex(colorLabelIndex);
    node->setX(x);
    node->setY(y);
    node->setName(name);

    if (! id.isNull())          // if no uuid in file, new one has been generated already
        node->setUuid(id);

    if (node->inherits("KisLayer") || node->inherits("KisColorizeMask")) {
        PkString compositeOpName = element.attribute(COMPOSITE_OP, "normal");
        node->setCompositeOpId(compositeOpName);

        if (m_d->kritaVersion < PkVersionNumber(5, 2) &&
            colorSpace->colorModelId() == CMYKAColorModelID &&
            subtractiveBlendingModesInCmyk().contains(compositeOpName)) {

            m_d->warningMessages <<
                i18n("Layer \"%1\" has blending mode \"%2\" that has changed its "
                    "behavior for CMYK color in Krita 5.2. Please check the "
                    "result and consider enabling legacy \"Additive\" algorithm in "
                    "Settings->Configure Krita->General->Tools->CMYK blending mode",
                    name, KoCompositeOpRegistry::instance().getKoID(compositeOpName).name());
        }
    }

    if (node->inherits("KisLayer")) {
        KisLayer* layer           = dynamic_cast<KisLayer*>(node.data());
        PkBitArray channelFlags    = stringToFlags(element.attribute(CHANNEL_FLAGS, ""), colorSpace->channelCount());
        layer->setChannelFlags(channelFlags);

        if (element.hasAttribute(LAYER_STYLE_UUID)) {
            PkString uuidString = element.attribute(LAYER_STYLE_UUID);
            PkNodeId uuid(uuidString);
            if (!uuid.isNull()) {
                KisPSDLayerStyleSP dumbLayerStyle(new KisPSDLayerStyle());
                dumbLayerStyle->setUuid(uuid);
                layer->setLayerStyle(dumbLayerStyle->cloneWithResourcesSnapshot(KisGlobalResourcesInterface::instance(), 0));
            } else {
                warnKrita << "WARNING: Layer style for layer" << layer->name() << "contains invalid UUID" << uuidString;
            }
        }
    }

    if (node->inherits("KisGroupLayer")) {
        if (element.hasAttribute(PASS_THROUGH_MODE)) {
            bool value = element.attribute(PASS_THROUGH_MODE, "0") != "0";

            KisGroupLayer *group = dynamic_cast<KisGroupLayer*>(node.data());
            group->setPassThroughMode(value);
        }
    }

    if (node->inherits("KisShapeLayer")) {
        if (element.hasAttribute(ANTIALIASED)) {
            bool value = element.attribute(ANTIALIASED, "0") != "0";

            KisShapeLayer *shapeLayer = dynamic_cast<KisShapeLayer*>(node.data());
            shapeLayer->setAntialiased(value);
        }
    }


    const bool timelineEnabled = element.attribute(VISIBLE_IN_TIMELINE, "0") == "0" ? false : true;
    node->setPinnedToTimeline(timelineEnabled);

    if (node->inherits("KisPaintLayer")) {
        KisPaintLayer* layer = dynamic_cast<KisPaintLayer*>(node.data());
        PkBitArray channelLockFlags = stringToFlags(element.attribute(CHANNEL_LOCK_FLAGS, ""), colorSpace->channelCount());
        layer->setChannelLockFlags(channelLockFlags);

        bool onionEnabled = element.attribute(ONION_SKIN_ENABLED, "0") == "0" ? false : true;
        layer->setOnionSkinEnabled(onionEnabled);
    }

    if (element.attribute(FILE_NAME).isEmpty()) {
        m_d->layerFilenames[node.data()] = name;
    }
    else {
        m_d->layerFilenames[node.data()] = element.attribute(FILE_NAME);
    }

    if (element.hasAttribute("selected") && element.attribute("selected") == "true")  {
        m_d->selectedNodes.append(node);
    }

    if (element.hasAttribute(KEYFRAME_FILE)) {
        m_d->keyframeFilenames.insert(node.data(), element.attribute(KEYFRAME_FILE));
    }

    return node;
}


KisNodeSP KisKraLoader::loadPaintLayer(const PkXmlElement& element, KisImageSP image,
                                       const PkString& name, const KoColorSpace* cs, quint32 opacity)
{
    Q_UNUSED(element);
    KisPaintLayer* layer;

    layer = new KisPaintLayer(image, name, opacity, cs);
    Q_CHECK_PTR(layer);
    return layer;

}

KisNodeSP KisKraLoader::loadFileLayer(const PkXmlElement& element, KisImageSP image, const PkString& name, quint32 opacity, const KoColorSpace *fallbackColorSpace)
{
    PkString filename = element.attribute("source", PkString());
    if (filename.isEmpty()) return 0;
    bool scale = (element.attribute("scale", "true")  == "true");
    int scalingMethod = element.attribute("scalingmethod", "-1").toInt();
    if (scalingMethod < 0) {
        if (scale) {
            scalingMethod = KisFileLayer::ToImagePPI;
        }
        else {
            scalingMethod = KisFileLayer::None;
        }
    }
    PkString scalingFilter = element.attribute("scalingfilter", "Bicubic");

    PkString documentPath;
    if (m_d->document) {
        documentPath = m_d->document->path();
    }
    const std::filesystem::path baseDir =
        std::filesystem::absolute(std::filesystem::path(documentPath.PkToUtf8())).parent_path();
    const PkString basePath = PkString(baseDir.string().c_str());

    std::filesystem::path fullPath;
#ifndef Q_OS_ANDROID
    fullPath = (baseDir / std::filesystem::path(filename.PkToUtf8()).lexically_normal()).lexically_normal();
#else
    fullPath = std::filesystem::path(filename.PkToUtf8());
#endif
    if (!std::filesystem::exists(fullPath)) {
        if (m_d->feedbackInterface) {
            PkString chosenUrl;
            m_d->feedbackInterface->askUser([&](PkWidget *parent) {
                (void)i18nc(
                            "@info",
                            "The file associated to a file layer with the name \"%1\" is not found.\n\n"
                            "Expected path:\n"
                            "%2\n\n"
                            "Do you want to locate it manually?", name, fullPath.string().c_str());

                chosenUrl = KisImportExportManager::getUriForAdditionalFile(fullPath.string().c_str(), parent);
                return !chosenUrl.isEmpty();
            });

            if (!chosenUrl.isEmpty()) {
                if (!std::filesystem::exists(baseDir)) {
                    filename = chosenUrl;
                } else {
                    filename = PkString(std::filesystem::relative(chosenUrl.PkToUtf8(), baseDir).string().c_str());
                }
            }
        }
    }

    KisLayer *layer = new KisFileLayer(image, basePath, filename, (KisFileLayer::ScalingMethod)scalingMethod, scalingFilter, name, opacity, fallbackColorSpace);
    Q_CHECK_PTR(layer);

    return layer;
}

KisNodeSP KisKraLoader::loadGroupLayer(const PkXmlElement& element, KisImageSP image,
                                       const PkString& name, const KoColorSpace* cs, quint32 opacity)
{
    Q_UNUSED(element);
    KisGroupLayer* layer;

    layer = new KisGroupLayer(image, name, opacity, cs);
    Q_CHECK_PTR(layer);

    return layer;

}

KisNodeSP KisKraLoader::loadAdjustmentLayer(const PkXmlElement& element, KisImageSP image,
                                            const PkString& name, const KoColorSpace* cs, quint32 opacity)
{
    // XXX: do something with filterversion?
    Q_UNUSED(cs);
    PkString attr;
    KisAdjustmentLayer* layer;
    PkString filtername;
    PkString legacy = filtername;

    if ((filtername = element.attribute(FILTER_NAME)).isEmpty()) {
        // XXX: Invalid adjustment layer! We should warn about it!
        warnFile << "No filter in adjustment layer";
        return 0;
    }

    //get deprecated filters.
    if (filtername=="brightnesscontrast") {
        legacy = filtername;
        filtername = "perchannel";
    }
    if (filtername=="left edge detections"
            || filtername=="right edge detections"
            || filtername=="top edge detections"
            || filtername=="bottom edge detections") {
        legacy = filtername;
        filtername = "edge detection";
    }

    KisFilterSP f = KisFilterRegistry::instance()->value(filtername);
    if (!f) {
        warnFile << "No filter for filtername" << filtername << "";
        return 0; // XXX: We don't have this filter. We should warn about it!
    }

    KisFilterConfigurationSP  kfc = f->defaultConfiguration(KisGlobalResourcesInterface::instance());
    kfc->createLocalResourcesSnapshot();
    kfc->setProperty("legacy", legacy);
    if (legacy=="brightnesscontrast") {
        kfc->setProperty("colorModel", cs->colorModelId().id());
    }

    // We'll load the configuration and the selection later.
    layer = new KisAdjustmentLayer(image, name, kfc, 0);
    Q_CHECK_PTR(layer);

    layer->setOpacity(opacity);

    return layer;

}


KisNodeSP KisKraLoader::loadShapeLayer(const PkXmlElement& element, KisImageSP image,
                                       const PkString& name, const KoColorSpace* cs, quint32 opacity)
{

    Q_UNUSED(element);
    Q_UNUSED(cs);

    PkString attr;
    KoShapeControllerBase * shapeController = 0;
    if (m_d->document) {
        shapeController = m_d->document->shapeController();
    }
    KisShapeLayer* layer = new KisShapeLayer(shapeController, image, name, opacity);
    Q_CHECK_PTR(layer);

    return layer;

}


KisNodeSP KisKraLoader::loadGeneratorLayer(const PkXmlElement& element, KisImageSP image,
                                           const PkString& name, const KoColorSpace* cs, quint32 opacity)
{
    Q_UNUSED(cs);
    // XXX: do something with generator version?
    KisGeneratorLayer* layer;
    PkString generatorname = element.attribute(GENERATOR_NAME);

    if (generatorname.isEmpty()) {
        // XXX: Invalid generator layer! We should warn about it!
        warnFile << "No generator in generator layer";
        return 0;
    }

    KisGeneratorSP generator = KisGeneratorRegistry::instance()->value(generatorname);
    if (!generator) {
        warnFile << "No generator for generatorname" << generatorname << "";
        return 0; // XXX: We don't have this generator. We should warn about it!
    }

    KisFilterConfigurationSP  kgc = generator->defaultConfiguration(KisGlobalResourcesInterface::instance());
    kgc->createLocalResourcesSnapshot();

    // We'll load the configuration and the selection later.
    layer = new KisGeneratorLayer(image, name, kgc, 0);
    Q_CHECK_PTR(layer);

    layer->setOpacity(opacity);

    return layer;

}

KisNodeSP KisKraLoader::loadCloneLayer(const PkXmlElement& element, KisImageSP image,
                                       const PkString& name, const KoColorSpace* cs, quint32 opacity)
{
    Q_UNUSED(cs);

    KisCloneLayerSP layer = new KisCloneLayer(0, image, name, opacity);

    KisNodeUuidInfo info;
    if (! (element.attribute(CLONE_FROM_UUID)).isEmpty()) {
        info = KisNodeUuidInfo(PkNodeId(element.attribute(CLONE_FROM_UUID)));
    } else {
        if ((element.attribute(CLONE_FROM)).isEmpty()) {
            return 0;
        } else {
            info = KisNodeUuidInfo(element.attribute(CLONE_FROM));
        }
    }
    layer->setCopyFromInfo(info);

    if ((element.attribute(CLONE_TYPE)).isEmpty()) {
        return 0;
    } else {
        layer->setCopyType((CopyLayerType) element.attribute(CLONE_TYPE).toInt());
    }

    return layer;
}


KisNodeSP KisKraLoader::loadFilterMask(KisImageSP image, const PkXmlElement& element)
{
    PkString attr;
    KisFilterMask* mask;
    PkString filtername;

    // XXX: should we check the version?

    if ((filtername = element.attribute(FILTER_NAME)).isEmpty()) {
        // XXX: Invalid filter layer! We should warn about it!
        warnFile << "No filter in filter layer";
        return 0;
    }

    KisFilterSP f = KisFilterRegistry::instance()->value(filtername);
    if (!f) {
        warnFile << "No filter for filtername" << filtername << "";
        return 0; // XXX: We don't have this filter. We should warn about it!
    }

    KisFilterConfigurationSP  kfc = f->defaultConfiguration(KisGlobalResourcesInterface::instance());
    kfc->createLocalResourcesSnapshot();

    // We'll load the configuration and the selection later.
    mask = new KisFilterMask(image);
    mask->setFilter(kfc);
    Q_CHECK_PTR(mask);

    return mask;
}

KisNodeSP KisKraLoader::loadTransformMask(KisImageSP image, const PkXmlElement& element)
{
    Q_UNUSED(element);

    KisTransformMask* mask;

    /**
     * We'll load the transform configuration later on a stage
     * of binary data loading
     */
    mask = new KisTransformMask(image, "");
    Q_CHECK_PTR(mask);

    return mask;
}

KisNodeSP KisKraLoader::loadTransparencyMask(KisImageSP image, const PkXmlElement& element)
{
    Q_UNUSED(element);
    KisTransparencyMask* mask = new KisTransparencyMask(image, "");
    Q_CHECK_PTR(mask);

    return mask;
}

KisNodeSP KisKraLoader::loadSelectionMask(KisImageSP image, const PkXmlElement& element)
{
    KisSelectionMaskSP mask = new KisSelectionMask(image);
    bool active = element.attribute(ACTIVE, "1") == "0" ? false : true;
    mask->setActive(active);
    Q_CHECK_PTR(mask);

    return mask;
}

KisNodeSP KisKraLoader::loadColorizeMask(KisImageSP image, const PkXmlElement& element, const KoColorSpace *colorSpace)
{
    KisColorizeMaskSP mask = new KisColorizeMask(image, "");
    const bool editKeystrokes = element.attribute(COLORIZE_EDIT_KEYSTROKES, "1") == "0" ? false : true;
    const bool showColoring = element.attribute(COLORIZE_SHOW_COLORING, "1") == "0" ? false : true;

    KisBaseNode::PropertyList props = mask->sectionModelProperties();
    KisLayerPropertiesIcons::setNodeProperty(&props, KisLayerPropertiesIcons::colorizeEditKeyStrokes, editKeystrokes);
    KisLayerPropertiesIcons::setNodeProperty(&props, KisLayerPropertiesIcons::colorizeShowColoring, showColoring);
    mask->setSectionModelProperties(props);

    const bool useEdgeDetection = KisDomUtils::toInt(element.attribute(COLORIZE_USE_EDGE_DETECTION, "0"));
    const qreal edgeDetectionSize = KisDomUtils::toDouble(element.attribute(COLORIZE_EDGE_DETECTION_SIZE, "4"));
    const qreal radius = KisDomUtils::toDouble(element.attribute(COLORIZE_FUZZY_RADIUS, "0"));
    const int cleanUp = KisDomUtils::toInt(element.attribute(COLORIZE_CLEANUP, "0"));
    const bool limitToDevice = KisDomUtils::toInt(element.attribute(COLORIZE_LIMIT_TO_DEVICE, "0"));

    mask->setUseEdgeDetection(useEdgeDetection);
    mask->setEdgeDetectionSize(edgeDetectionSize);
    mask->setFuzzyRadius(radius);
    mask->setCleanUpAmount(qreal(cleanUp) / 100.0);
    mask->setLimitToDeviceBounds(limitToDevice);

    delete mask->setColorSpace(colorSpace);

    return mask;
}

void KisKraLoader::loadCompositions(const PkXmlElement& elem, KisImageSP image)
{
    PkXmlNode child;

    for (child = elem.firstChild(); !child.isNull(); child = child.nextSibling()) {

        PkXmlElement e = child.toElement();
        PkString name = e.attribute("name");
        bool exportEnabled = e.attribute("exportEnabled", "1") == "0" ? false : true;

        KisLayerCompositionSP composition(new KisLayerComposition(image, name));
        composition->setExportEnabled(exportEnabled);

        PkXmlNode value;
        for (value = child.lastChild(); !value.isNull(); value = value.previousSibling()) {
            PkXmlElement e = value.toElement();
            PkNodeId uuid(e.attribute("uuid"));
            bool visible = e.attribute("visible", "1") == "0" ? false : true;
            composition->setVisible(uuid, visible);
            bool collapsed = e.attribute("collapsed", "1") == "0" ? false : true;
            composition->setCollapsed(uuid, collapsed);
        }

        image->addComposition(composition);
    }
}

void KisKraLoader::loadAssistantsList(const PkXmlElement &elem)
{
    PkXmlNode child;
    for (child = elem.firstChild(); !child.isNull(); child = child.nextSibling()) {
        PkXmlElement e = child.toElement();
        PkString type = e.attribute("type");
        PkString file_name = e.attribute("filename");
        m_d->assistantsFilenames.insert(file_name,type);
    }
}

void KisKraLoader::loadGrid(const PkXmlElement& elem)
{
    PkXmlDocument dom;
    dom.appendChild(dom.importNode(elem, true));
    PkXmlElement domElement = dom.firstChildElement();

    KisGridConfig config;
    config.loadStaticData();
    config.loadDynamicDataFromXml(domElement);
    m_d->document->setGridConfig(config);
}

void KisKraLoader::loadGuides(const PkXmlElement& elem)
{
    PkXmlDocument dom;
    dom.appendChild(dom.importNode(elem, true));
    PkXmlElement domElement = dom.firstChildElement();

    KisGuidesConfig guides;
    guides.loadFromXml(domElement);
    m_d->document->setGuidesConfig(guides);
}

void KisKraLoader::loadMirrorAxis(const PkXmlElement &elem)
{
    PkXmlDocument dom;
    dom.appendChild(dom.importNode(elem, true));
    PkXmlElement domElement = dom.firstChildElement();

    KisMirrorAxisConfig mirrorAxis;
    mirrorAxis.loadFromXml(domElement);
    m_d->document->setMirrorAxisConfig(mirrorAxis);
}

void KisKraLoader::loadStoryboardItemList(const PkXmlElement& elem)
{
    PkXmlNode child;
    for (child = elem.firstChild(); !child.isNull(); child = child.nextSibling()) {
        PkXmlElement e = child.toElement();
        if (e.tagName() == "storyboarditem") {
            StoryboardItemSP item = toQShared( new StoryboardItem() );
            item->loadXML(e);
            m_d->storyboardItemList.append(item);
        }
    }
}

void KisKraLoader::loadStoryboardCommentList(const PkXmlElement& elem)
{
    PkXmlNode child;
    for (child = elem.firstChild(); !child.isNull(); child = child.nextSibling()) {
        PkXmlElement e = child.toElement();
        if (e.tagName() == "storyboardcomment") {
            StoryboardComment comment;
            if (e.hasAttribute("visibility")) {
                comment.visibility = e.attribute("visibility").toInt();
            }
            if (e.hasAttribute("name")) {
                comment.name = e.attribute("name");
            }
            m_d->storyboardCommentList.append(comment);
        }
    }
}

void KisKraLoader::loadAudioXML(PkXmlDocument &xmlDoc, PkXmlElement &xmlElement, KisDocument *kisDoc)
{
    Q_UNUSED(xmlDoc);
    PkXmlNode audioClip = xmlElement.firstChild();
    if (audioClip.nodeName() == "audioClips") {
        PkXmlElement audioClipElement = audioClip.toElement();
        PkVector<std::filesystem::path> clipFiles;
        qreal volume = 1.0;
        PkXmlNode clip;
        for (clip = audioClipElement.firstChild(); !clip.isNull(); clip = clip.nextSibling()) {
            PkXmlElement clipElem = clip.toElement();

            if (clipElem.hasAttribute("filePath")) {
                std::filesystem::path f(clipElem.attribute("filePath").PkToUtf8());
                if (std::filesystem::exists(f)) {
                    clipFiles.append(f);
                }
            }

            if (clipElem.hasAttribute("volume")) {
                volume = clipElem.attribute("volume").toDouble();
            }
        }

        kisDoc->setAudioTracks(clipFiles);
        kisDoc->setAudioVolume(volume);
    }
}

KisNodeSP KisKraLoader::loadReferenceImagesLayer(const PkXmlElement &elem, KisImageSP image)
{
    KisSharedPtr<KisReferenceImagesLayer> layer =
            new KisReferenceImagesLayer(m_d->document->shapeController(), image);

    m_d->document->setReferenceImagesLayer(layer, false);

    for (PkXmlElement child = elem.firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) {
        if (child.nodeName().toLower() == "referenceimage") {
            auto* reference = KisReferenceImage::fromXml(child);
            reference->setZIndex(layer->shapes().size());
            layer->addShape(reference);
        }
    }

    return layer;
}

PkList<KoColor> KisKraLoader::loadKoColors(const PkXmlElement &colorElement) const
{
    PkList<KoColor> colors;
    PkXmlNodeList colorNodes = colorElement.childNodes();
    colors.reserve(colorNodes.size());

    for (int k = 0; k < colorNodes.size(); k++) {
        PkXmlElement colorElement = colorNodes.at(k).toElement();
        KoColor color = KoColor::fromXML(colorElement, Integer16BitsColorDepthID.id());
        colors.push_back(color);
    }

    return colors;
}
