/* This file is part of the KDE project
 * Copyright 2008 (C) Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <klocalizedstring.h>
#include "kis_kra_saver.h"

#include "kis_kra_tags.h"
#include "kis_kra_save_visitor.h"
#include "kis_kra_savexml_visitor.h"

#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkString.h>
#include <PkStringList.h>

#include <PkMemoryStream.h>

#include <KoDocumentInfo.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorSpace.h>
#include <KoColorProfile.h>
#include <KoColor.h>
#include <KoColorSet.h>
#include <KoStore.h>
#include <KoStoreDevice.h>
#include <KisResourceTypes.h>
#include <KisResourceModel.h>
#include <kis_annotation.h>
#include <kis_image.h>
#include <kis_image_animation_interface.h>
#include <KisImportExportManager.h>
#include <kis_group_layer.h>
#include <kis_layer.h>
#include <kis_adjustment_layer.h>
#include <kis_layer_composition.h>
#include <KisPngCodec.h>
#include <kis_image_config.h>
#include "kis_keyframe_channel.h"
#include <kis_time_span.h>
#include "KisDocument.h"
#include <string>
#include "kis_dom_utils.h"
#include "kis_grid_config.h"
#include "kis_guides_config.h"
#include "KisProofingConfiguration.h"
#include "kis_asl_layer_style_serializer.h"

#include <KisMirrorAxisConfig.h>

#include <filesystem>


using namespace KRA;

struct KisKraSaver::Private
{
    KisDocument* doc {nullptr};
    PkMap<const KisNode*, PkString> nodeFileNames;
    PkMap<const KisNode*, PkString> keyframeFilenames;
    PkString imageName;
    PkString filename;
    PkStringList errorMessages;
    PkStringList warningMessages;
    PkStringList specialAnnotations;
    bool addMergedImage {false};
    PkList<KoResourceLoadResult> linkedDocumentResources;

    Private() {
        specialAnnotations << "exif" << "icc";
    }

};

KisKraSaver::KisKraSaver(KisDocument* document, const PkString &filename, bool addMergedImage)
    : m_d(new Private)
{
    m_d->doc = document;
    m_d->filename = filename;
    m_d->addMergedImage = addMergedImage;
    m_d->linkedDocumentResources = document->linkedDocumentResources();

    m_d->imageName = m_d->doc->documentInfo()->aboutInfo("title");
    if (m_d->imageName.isEmpty()) {
        m_d->imageName = i18n("Unnamed");
    }
}

KisKraSaver::~KisKraSaver()
{
    delete m_d;
}

PkXmlElement KisKraSaver::saveXML(PkXmlDocument& doc,  KisImageSP image)
{
    PkXmlElement imageElement = doc.createElement("IMAGE");

    Q_ASSERT(image);
    imageElement.setAttribute(NAME, m_d->imageName);
    imageElement.setAttribute(MIME, NATIVE_MIMETYPE);
    imageElement.setAttribute(WIDTH, KisDomUtils::toString(image->width()));
    imageElement.setAttribute(HEIGHT, KisDomUtils::toString(image->height()));
    imageElement.setAttribute(COLORSPACE_NAME, image->colorSpace()->id());
    imageElement.setAttribute(DESCRIPTION, m_d->doc->documentInfo()->aboutInfo("comment"));
    // XXX: Save profile as blob inside the image, instead of the product name.
    if (image->profile() && image->profile()-> valid()) {
        imageElement.setAttribute(PROFILE, image->profile()->name());
    }
    imageElement.setAttribute(X_RESOLUTION, KisDomUtils::toString(image->xRes()*72.0));
    imageElement.setAttribute(Y_RESOLUTION, KisDomUtils::toString(image->yRes()*72.0));

    //now the proofing options:
    if (image->proofingConfiguration()) {
        imageElement.setAttribute(PROOFINGPROFILENAME,
                                  KisDomUtils::toString(image->proofingConfiguration()->proofingProfile));
        imageElement.setAttribute(PROOFINGMODEL, KisDomUtils::toString(image->proofingConfiguration()->proofingModel));
        imageElement.setAttribute(PROOFINGDEPTH, KisDomUtils::toString(image->proofingConfiguration()->proofingDepth));
        imageElement.setAttribute(PROOFINGINTENT,
                                  KisDomUtils::toString(image->proofingConfiguration()->conversionIntent));
        imageElement.setAttribute(PROOFINGDISPLAYINTENT,
                                  KisDomUtils::toString(image->proofingConfiguration()->displayIntent));
        bool bcp = image->proofingConfiguration()->useBlackPointCompensationFirstTransform;
        imageElement.setAttribute(PROOFINGBLACKPOINTCOMPENSATION, bcp ? "true" : "false");
        bcp = image->proofingConfiguration()->displayFlags.testFlag(
            KoColorConversionTransformation::BlackpointCompensation);
        imageElement.setAttribute(PROOFINGDISPLAYBLACKPOINTCOMPENSATION, bcp ? "true" : "false");
        const PkString mode = [&]() {
            switch (image->proofingConfiguration()->displayMode) {
            case KisProofingConfiguration::Monitor:
                return "monitor";
            case KisProofingConfiguration::Paper:
                return "paper";
            case KisProofingConfiguration::Custom:
                return "custom";
            }
            return "custom";
        }();
        imageElement.setAttribute(PROOFINGDISPLAYMODE, mode);
        imageElement.setAttribute(PROOFINGADAPTATIONSTATE,
                                  KisDomUtils::toString(image->proofingConfiguration()->legacyAdaptationState()));
    }

    quint32 count = 1; // We don't save the root layer, but it does count
    KisSaveXmlVisitor visitor(doc, imageElement, count, m_d->filename, true);
    visitor.setSelectedNodes({m_d->doc->preActivatedNode()});

    image->rootLayer()->accept(visitor);
    m_d->errorMessages.append(visitor.errorMessages());

    m_d->nodeFileNames = visitor.nodeFileNames();
    m_d->keyframeFilenames = visitor.keyframeFileNames();

    saveBackgroundColor(doc, imageElement, image);
    saveAssistantsGlobalColor(doc, imageElement);
    saveWarningColor(doc, imageElement, image);
    saveCompositions(doc, imageElement, image);
    saveAssistantsList(doc, imageElement);
    saveGrid(doc, imageElement);
    saveGuides(doc, imageElement);
    saveMirrorAxis(doc, imageElement);
    saveColorHistory(doc, imageElement);
    saveResourcesToXML(doc, imageElement);

    // Redundancy -- Save animation metadata in XML to prevent data loss for the time being...
    PkXmlElement animationElement = doc.createElement("animation");
    KisDomUtils::saveValue(&animationElement, "framerate", image->animationInterface()->framerate());
    KisDomUtils::saveValue(&animationElement, "range", image->animationInterface()->documentPlaybackRange());
    KisDomUtils::saveValue(&animationElement, "currentTime", image->animationInterface()->currentUITime());
    imageElement.appendChild(animationElement);

    vKisAnnotationSP_it beginIt = image->beginAnnotations();
    vKisAnnotationSP_it endIt = image->endAnnotations();

    if (beginIt != endIt) {
        PkXmlElement annotationsElement = doc.createElement(ANNOTATIONS);
        vKisAnnotationSP_it it = beginIt;
        while (it != endIt) {
            if (!(*it) || (*it)->type().isEmpty()) {
                it++;
                continue;
            }
            PkString type = (*it)->type();

            if (!m_d->specialAnnotations.contains(type)) {

                PkString description = (*it)->description();
                PkXmlElement annotationElement = doc.createElement(ANNOTATION);
                annotationsElement.appendChild(annotationElement);
                annotationElement.setAttribute("type", type);
                annotationElement.setAttribute("description", description);
            }
            it++;
        }
        imageElement.appendChild(annotationsElement);
    }


    return imageElement;
}

bool KisKraSaver::saveResources(KoStore *store, KisImageSP image, const PkString &uri)
{
    Q_UNUSED(image);
    Q_UNUSED(uri);

    PkList<KoResourceLoadResult> embeddedResources = m_d->linkedDocumentResources;

    Q_FOREACH (const KoResourceLoadResult &result, embeddedResources) {
        KIS_SAFE_ASSERT_RECOVER(result.type() != KoResourceLoadResult::ExistingResource) { continue; }

        if (result.type() == KoResourceLoadResult::FailedLink) {
            m_d->warningMessages << i18nc("Error message when saving a .kra file", "Could not export resource for embedding: %1", result.signature().filename);
            continue;
        }

        KoEmbeddedResource resource = result.embeddedResource();

        PkString path = RESOURCE_PATH + "/" + resource.signature().type;

        if (resource.signature().type == ResourceType::Palettes) {
            path = m_d->imageName + PALETTE_PATH;
        }

        const PkString fileName = resource.signature().filename;

        if (!store->open(path + "/" + fileName)) {
            m_d->warningMessages << i18nc("Error message when saving a .kra file", "Could not write resource: %1", result.signature().filename);
            continue;
        }

        // we first read into a buffer to make sure the save operation is transactional,
        // that is, either resource is saves correctly, or the file is left empty.
        PkByteArray ba = resource.data();

        qint64 nwritten = 0;
        if (!ba.isEmpty()) {
            nwritten = store->write(ba);
        } else {
            m_d->warningMessages << i18nc("Error message when saving a .kra file", "Written resource is empty: %1", result.signature().filename);
        }

        store->close();

        if (nwritten != ba.size()) {
            m_d->warningMessages << i18nc("Error message when saving a .kra file", "Written resource is incomplete: %1", result.signature().filename);
        }
    }

    return true;
}

bool KisKraSaver::saveStoryboard(KoStore *store, KisImageSP image, const PkString &uri)
{
    Q_UNUSED(image);
    Q_UNUSED(uri);

    bool success = true;
    if (m_d->doc->getStoryboardItemList().count() == 0) {
        return true;
    } else {
        if (!store->open(m_d->imageName + STORYBOARD_PATH + "index.xml")) {
            m_d->errorMessages << i18nc("Error message when saving a .kra file", "Could not save storyboards.");
            return false;
        }

        PkXmlDocument storyboardDocument = m_d->doc->createDomDocument("storyboard-info", "1.1");
        PkXmlElement root = storyboardDocument.documentElement();
        saveStoryboardToXML(storyboardDocument, root);

        PkByteArray ba = storyboardDocument.toByteArray();
        qint64 nwritten = 0;
        if (!ba.isEmpty()) {
            nwritten = store->write(ba);
        } else {
            success = false;
            qWarning() << "Could not save storyboard data to a byte array!";
        }

        bool r = store->close();
        success = success && r && (nwritten == ba.size());
    }

    if (!success) {
        m_d->errorMessages << i18nc("Error message when saving a .kra file", "Could not save storyboards.");
        return false;
    }

    return success;
}

bool KisKraSaver::saveAnimationMetadata(KoStore *store, KisImageSP image, const PkString &uri)
{
    Q_UNUSED(uri);

    if (!store->open(m_d->imageName + ANIMATION_METADATA_PATH + "index.xml")) {
        m_d->errorMessages << i18nc("Error message when saving a .kra file", "Could not save animation meta data.");
        return false;
    }

    PkXmlDocument animationDocument = m_d->doc->createDomDocument("animation-metadata", "1.1");
    PkXmlElement root = animationDocument.documentElement();
    saveAnimationMetadataToXML(animationDocument, root, image);

    bool success = true;

    PkByteArray ba = animationDocument.toByteArray();
    qint64 nwritten = 0;
    if (!ba.isEmpty()) {
        nwritten = store->write(ba);
    } else {
        qWarning() << "Could not save animation meta data to a byte array!";
        success = false;
    }

    bool r = store->close();

    success = success && r && (nwritten == ba.size());

    if (!success) {
        m_d->errorMessages << i18nc("Error message when saving a .kra file", "Could not save animation meta data.");
        return false;
    }

    return true;
}

bool KisKraSaver::saveAudio(KoStore *store)
{
    if (m_d->doc->getAudioTracks().isEmpty())
        return true;

    if (!store->open(m_d->imageName + AUDIO_PATH + "index.xml")) {
        m_d->errorMessages << i18nc("Error message when saving a .kra file", "Could not save audio meta data.");
        return false;
    }

    PkXmlDocument audioDocument = m_d->doc->createDomDocument("audio-info", "1.1");
    PkXmlElement root = audioDocument.documentElement();
    saveAudioXML(audioDocument, root);

    bool success = true;
    PkByteArray byteArray = audioDocument.toByteArray();
    qint64 bytesWriteCount = 0;
    if (!byteArray.isEmpty()) {
        bytesWriteCount = store->write(byteArray);
    } else {
        qWarning() << "Could not save audio data to a byte array!";
        success = false;
    }

    bool closeOK = store->close();

    success = success && closeOK && (bytesWriteCount == byteArray.size());

    if (!success) {
        m_d->errorMessages << i18nc("Error message when saving a .kra file", "Could not save audio meta data.");
        return false;
    }

    return true;
}

void KisKraSaver::saveResourcesToXML(PkXmlDocument &doc, PkXmlElement &element)
{
    PkXmlElement ePalette = doc.createElement(PALETTES);
    PkXmlElement eResources = doc.createElement(RESOURCES);

    Q_FOREACH (const KoResourceLoadResult resource, m_d->linkedDocumentResources) {
        // all warnings will be issued in KisKraSaver::saveResources()
        if (resource.type() != KoResourceLoadResult::EmbeddedResource) continue;

        KoResourceSignature sig = resource.signature();

        PkXmlElement eResource = doc.createElement("resource");
        eResource.setAttribute("type", sig.type);
        eResource.setAttribute("name", sig.name);
        eResource.setAttribute("filename", sig.filename);
        eResource.setAttribute("md5sum", sig.md5sum);

        if (sig.type == ResourceType::Palettes) {
            ePalette.appendChild(eResource);
        }
        else {
            eResources.appendChild(eResource);

        }
    }
    element.appendChild(ePalette);
    element.appendChild(eResources);
}

void KisKraSaver::saveStoryboardToXML(PkXmlDocument& doc, PkXmlElement &element)
{
    //saving storyboard comments
    PkXmlElement eCommentList = doc.createElement("StoryboardCommentList");
    for (StoryboardComment comment: m_d->doc->getStoryboardCommentsList()) {
        PkXmlElement commentElement = doc.createElement("storyboardcomment");
        commentElement.setAttribute("name", comment.name);
        commentElement.setAttribute("visibility", comment.visibility);
        eCommentList.appendChild(commentElement);
    }
    element.appendChild(eCommentList);

    //saving storyboard items
    PkXmlElement eItemList = doc.createElement("StoryboardItemList");
    for (StoryboardItemSP item : m_d->doc->getStoryboardItemList()) {
        PkXmlElement eItem =  item->toXML(doc);
        eItemList.appendChild(eItem);
    }
    element.appendChild(eItemList);
}

void KisKraSaver::saveAnimationMetadataToXML(PkXmlDocument &doc, PkXmlElement &element, KisImageSP image)
{
    KisDomUtils::saveValue(&element, "framerate", image->animationInterface()->framerate());
    KisDomUtils::saveValue(&element, "range", image->animationInterface()->documentPlaybackRange());
    KisDomUtils::saveValue(&element, "currentTime", image->animationInterface()->currentUITime());

    {
        PkXmlElement exportItemElem = doc.createElement("export-settings");
        KisDomUtils::saveValue(&exportItemElem, "sequenceFilePath", image->animationInterface()->exportSequenceFilePath());
        KisDomUtils::saveValue(&exportItemElem, "sequenceBaseName", image->animationInterface()->exportSequenceBaseName());
        KisDomUtils::saveValue(&exportItemElem, "sequenceInitialFrameNumber", image->animationInterface()->exportInitialFrameNumber());
        element.appendChild(exportItemElem);
    }
}

bool KisKraSaver::saveKeyframes(KoStore *store, const PkString &uri, bool external)
{
    PkMap<const KisNode*, PkString>::iterator it;

    for (it = m_d->keyframeFilenames.begin(); it != m_d->keyframeFilenames.end(); it++) {
        const KisNode *node = it.key();
        PkString filename = it.value();

        PkString location =
                (external ? PkString() : uri)
                + m_d->imageName + LAYER_PATH + filename;

        if (!saveNodeKeyframes(store, location, node)) {
            return false;
        }
    }

    return true;
}

bool KisKraSaver::saveNodeKeyframes(KoStore *store, PkString location, const KisNode *node)
{
    PkXmlDocument doc = KisDocument::createDomDocument("krita-keyframes", "keyframes", "1.0");
    PkXmlElement root = doc.documentElement();

    KisKeyframeChannel *channel;
    Q_FOREACH (channel, node->keyframeChannels()) {
        PkXmlElement element = channel->toXML(doc, m_d->nodeFileNames[node]);
        root.appendChild(element);
    }

    bool success = true;
    if (store->open(location)) {
        PkByteArray xml = doc.toByteArray();
        qint64 nwritten = store->write(xml);
        bool r = store->close();
        success = r && (nwritten == xml.size());
    } else {
        success = false;
    }
    if (!success) {
        m_d->errorMessages << i18nc("Error message on saving a .kra file", "Could not save keyframes.");
        return false;
    }

    return true;
}

bool KisKraSaver::saveBinaryData(KoStore* store, KisImageSP image, const PkString &uri, bool external, bool addMergedImage)
{
    PkString location;

    // Save the layers data
    KisKraSaveVisitor visitor(store, m_d->imageName, m_d->nodeFileNames);

    if (external)
        visitor.setExternalUri(uri);

    image->rootLayer()->accept(visitor);

    m_d->errorMessages.append(visitor.errorMessages());
    if (!m_d->errorMessages.isEmpty()) {
        return false;
    }

    bool success = true;
    bool r = true;
    qint64 nwritten = 0;

    // saving annotations
    bool savingAnnotationsSuccess = true;
    KisAnnotationSP annotation = image->annotation("exif");
    if (annotation) {
        location = external ? PkString() : uri;
        location += m_d->imageName + EXIF_PATH;
        if (store->open(location)) {
            nwritten = store->write(annotation->annotation());
            r = store->close();
            savingAnnotationsSuccess = savingAnnotationsSuccess && (nwritten == annotation->annotation().size()) && r;
        } else {
            savingAnnotationsSuccess = false;
        }
    }

    if (!savingAnnotationsSuccess) {
        m_d->errorMessages.append(i18nc("Saving .kra file error message", "Could not save annotations."));
    }

    success = success && savingAnnotationsSuccess;

    bool savingImageProfileSuccess = true;
    if (image->profile()) {
        const KoColorProfile *profile = image->profile();
        KisAnnotationSP annotation;
        if (profile) {
            PkByteArray profileRawData = profile->rawData();
            if (!profileRawData.isEmpty()) {
                if (profile->type() == "icc") {
                    annotation = new KisAnnotation(ICC, profile->name(), profile->rawData());
                } else {
                    annotation = new KisAnnotation(PROFILE, profile->name(), profile->rawData());
                }
            }
        }

        if (annotation) {
            location = external ? PkString() : uri;
            location += m_d->imageName + ICC_PATH;
            if (store->open(location)) {
                nwritten = store->write(annotation->annotation());
                r = store->close();
                savingImageProfileSuccess = savingImageProfileSuccess && (nwritten == annotation->annotation().size()) && r;
            } else {
                savingImageProfileSuccess = false;
            }
        }
    }

    if (!savingImageProfileSuccess) {
        m_d->errorMessages.append(i18nc("Saving .kra file error message", "Could not save image profile."));
    }
    success = success && savingImageProfileSuccess;

    //This'll embed the profile used for proofing into the kra file.
    bool savingSoftproofingProfileSuccess = true;
    if (image->proofingConfiguration()) {
        const KoColorProfile *proofingProfile =
            KoColorSpaceRegistry::instance()->profileByName(image->proofingConfiguration()->proofingProfile);
        if (proofingProfile && proofingProfile->valid()) {
            PkByteArray proofingProfileRaw = proofingProfile->rawData();
            if (!proofingProfileRaw.isEmpty()) {
                annotation = new KisAnnotation(ICCPROOFINGPROFILE, proofingProfile->name(), proofingProfile->rawData());
            }
        }
        if (annotation) {
            location = external ? PkString() : uri;
            location += m_d->imageName + ICC_PROOFING_PATH;
            if (store->open(location)) {
                nwritten = store->write(annotation->annotation());
                r = store->close();
                savingSoftproofingProfileSuccess =
                    savingSoftproofingProfileSuccess && (nwritten == annotation->annotation().size()) && r;
            } else {
                savingSoftproofingProfileSuccess = false;
            }
        }
    }

    if (!savingSoftproofingProfileSuccess) {
        m_d->errorMessages.append(i18nc("Saving .kra file error message", "Could not save softproofing color profile."));
    }

    success = success && savingSoftproofingProfileSuccess;

    // Save the remaining annotations
    vKisAnnotationSP_it beginIt = image->beginAnnotations();
    vKisAnnotationSP_it endIt = image->endAnnotations();

    bool savingRemainingAnnotationsSuccess = true;
    if (beginIt != endIt) {
        vKisAnnotationSP_it it = beginIt;
        while (it != endIt) {
            if (!(*it) || (*it)->type().isEmpty()) {
                it++;
                continue;
            }
            PkString type = (*it)->type();

            if (!m_d->specialAnnotations.contains(type)) {
                location = external ? PkString() : uri;
                location += m_d->imageName + ANNOTATIONS_PATH + type;
                if (store->open(location)) {
                    nwritten = store->write((*it)->annotation());
                    r = store->close();
                    savingRemainingAnnotationsSuccess = savingRemainingAnnotationsSuccess && (nwritten == (*it)->annotation().size()) && r;
                } else {
                    savingRemainingAnnotationsSuccess = false;
                }
            }
            it++;
        }
    }

    if (!savingRemainingAnnotationsSuccess) {
        m_d->errorMessages.append(i18nc("Saving .kra file error message", "Could not save additional annotations."));
    }

    success = success && savingRemainingAnnotationsSuccess;

    bool savingLayerStylesSuccess = true;
    {
        KisAslLayerStyleSerializer serializer;
        PkVector<KisPSDLayerStyleSP> stylesClones = serializer.collectAllLayerStyles(image->root());
        if (stylesClones.size() > 0) {
            location = external ? PkString() : uri;
            location += m_d->imageName + LAYER_STYLES_PATH;

            if (store->open(location)) {
                PkMemoryStream aslBuffer;
                if (aslBuffer.open(PkStream::WriteOnly)) {
                    serializer.setStyles(stylesClones);
                    serializer.saveToDevice(aslBuffer);
                    aslBuffer.close();
                    nwritten = store->write(PkByteArray(aslBuffer.data(), static_cast<int>(aslBuffer.size())));
                    savingLayerStylesSuccess = savingLayerStylesSuccess && (nwritten == aslBuffer.size());
                } else {
                    savingLayerStylesSuccess = false;
                }
                r = store->close();
                savingLayerStylesSuccess = savingLayerStylesSuccess && r;
            } else {
                savingLayerStylesSuccess = false;
            }
        }
    }

    if (!savingLayerStylesSuccess) {
        m_d->errorMessages.append(i18nc("Saving .kra file error message", "Could not save layer styles."));
    }

    success = success && savingLayerStylesSuccess;

    bool savingMergedImageSuccess = true;
    if (addMergedImage) {
        KisPaintDeviceSP dev = image->projection();
        store->setCompressionEnabled(false);
        r = KisPngCodec::saveDeviceToStore("mergedimage.png", image->bounds(), image->xRes(), image->yRes(), dev, store);
        savingMergedImageSuccess = savingMergedImageSuccess && r;
        store->setCompressionEnabled(KisImageConfig(true).compressKra());
    }

    if (!savingMergedImageSuccess) {
        m_d->errorMessages.append(i18nc("Saving .kra file error message", "Could not save merged image."));
    }

    success = success && savingMergedImageSuccess;

    r = saveAssistants(store, uri,external);
    success = success && r;

    return success;
}



PkStringList KisKraSaver::errorMessages() const
{
    return m_d->errorMessages;
}

PkStringList KisKraSaver::warningMessages() const
{
    return m_d->warningMessages;
}

void KisKraSaver::saveBackgroundColor(PkXmlDocument& doc, PkXmlElement& element, KisImageSP image)
{
    PkXmlElement e = doc.createElement(CANVASPROJECTIONCOLOR);
    KoColor color = image->defaultProjectionColor();
    PkByteArray colorData = PkByteArray::fromRawData((const char*)color.data(), color.colorSpace()->pixelSize());
    e.setAttribute(COLORBYTEDATA, PkString(colorData.toBase64()));
    element.appendChild(e);
}

void KisKraSaver::saveColorHistory(PkXmlDocument &doc, PkXmlElement &element)
{
    PkXmlElement colorsElement = doc.createElement(COLORHISTORY);
    saveKoColors(doc, colorsElement, m_d->doc->colorHistory());

    element.appendChild(colorsElement);
}

void KisKraSaver::saveAssistantsGlobalColor(PkXmlDocument& doc, PkXmlElement& element)
{
    PkXmlElement e = doc.createElement(GLOBALASSISTANTSCOLOR);
    PkString colorString = KisDomUtils::qColorToQString(m_d->doc->assistantsGlobalColor());
    e.setAttribute(SIMPLECOLORDATA, PkString(colorString));
    element.appendChild(e);
}

void KisKraSaver::saveWarningColor(PkXmlDocument& doc, PkXmlElement& element, KisImageSP image)
{
    if (image->proofingConfiguration()) {
        PkXmlElement e = doc.createElement(PROOFINGWARNINGCOLOR);
        KoColor color = image->proofingConfiguration()->warningColor;
        color.toXML(doc, e);
        element.appendChild(e);
    }
}

void KisKraSaver::saveCompositions(PkXmlDocument& doc, PkXmlElement& element, KisImageSP image)
{
    if (!image->compositions().isEmpty()) {
        PkXmlElement e = doc.createElement("compositions");
        Q_FOREACH (KisLayerCompositionSP composition, image->compositions()) {
            composition->save(doc, e);
        }
        element.appendChild(e);
    }
}

bool KisKraSaver::saveAssistants(KoStore* store, PkString uri, bool external)
{
    // 跨锁桩（S-09-f 恢复）：assistant 保存功能在 plugins/assistants 剥 Qt 完成后
    // 恢复。现在不写 .assistant 文件、不引用 assistant 相关符号。
    (void)store;
    (void)uri;
    (void)external;
    return true;
}

bool KisKraSaver::saveAssistantsList(PkXmlDocument& doc, PkXmlElement& element)
{
    // 跨锁桩（S-09-f 恢复）：不写 <assistants> 元素。
    (void)doc;
    (void)element;
    return true;
}

bool KisKraSaver::saveGrid(PkXmlDocument& doc, PkXmlElement& element)
{
    KisGridConfig config = m_d->doc->gridConfig();

    if (!config.isDefault()) {
        PkXmlElement gridElement = config.saveDynamicDataToXml(doc, "grid");
        element.appendChild(gridElement);
    }

    return true;
}

bool KisKraSaver::saveGuides(PkXmlDocument& doc, PkXmlElement& element)
{
    KisGuidesConfig guides = m_d->doc->guidesConfig();

    if (!guides.isDefault()) {
        PkXmlElement guidesElement = guides.saveToXml(doc, "guides");
        element.appendChild(guidesElement);
    }

    return true;
}

bool KisKraSaver::saveMirrorAxis(PkXmlDocument &doc, PkXmlElement &element)
{
    KisMirrorAxisConfig mirrorAxisConfig = m_d->doc->mirrorAxisConfig();

    if (!mirrorAxisConfig.isDefault()) {
        PkXmlElement mirrorAxisElement = mirrorAxisConfig.saveToXml(doc, MIRROR_AXIS);
        element.appendChild(mirrorAxisElement);
    }

    return true;
}

bool KisKraSaver::saveAudioXML(PkXmlDocument& doc, PkXmlElement& element)
{
    PkVector<std::filesystem::path> clips = m_d->doc->getAudioTracks();
    const qreal volume = m_d->doc->getAudioLevel();

    if (!clips.isEmpty()) {
        PkXmlElement audioClips = doc.createElement("audioClips");
        for (const auto &file : clips) {
            PkXmlElement clip = doc.createElement(PkString("Clip"));
            clip.setAttribute("filePath", PkString::PkFromUtf8(file.string().c_str(), static_cast<int>(file.string().size())));
            clip.setAttribute("volume", KisDomUtils::toString(volume));
            audioClips.appendChild(clip);
        }
        element.appendChild(audioClips);
    }

    return true;
}

bool KisKraSaver::saveKoColors(PkXmlDocument &doc, PkXmlElement &colorsElement,
                               const PkList<KoColor> &colors) const
{
    // Writes like <colors><RGB ../><RGB .. /> ... </colors>
    Q_FOREACH(const KoColor & color, colors) {
        color.toXML(doc, colorsElement);
    }
    return true;
}
