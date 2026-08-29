/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2009 Sven Langkamp <sven.langkamp@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <brushengine/kis_paintop_preset.h>

#include <PkString.h>
#include <PkStringList.h>
#include <PkList.h>
#include <PkScopedPointer.h>
#include <PkPointer.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkXmlCDATASection.h>
#include <PkStream.h>
#include <PkMemoryStream.h>

#include <KisResourceThumbnailCodec.h>
#include <asl/kis_asl_byte_utils.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <KisDirtyStateSaver.h>

#include "kis_dom_utils.h"

#include <brushengine/kis_paintop_settings.h>
#include "kis_paintop_registry.h"
#include "kis_painter.h"
#include <brushengine/kis_paint_information.h>
#include "kis_paint_device.h"
#include "kis_image.h"
#include "KisPaintOpPresetUpdateProxy.h"
#include <KisRequiredResourcesOperators.h>
#include <KoLocalStrokeCanvasResources.h>
#include <KisLocalStrokeResources.h>
#include <KisResourceModel.h>
#include "KisPaintopSettingsIds.h"
#include <KisResourceTypes.h>
#include <KisResourceModelProvider.h>
#include <krita_container_utils.h>
#include <KoResourceCacheInterface.h>

#include <KoStore.h>

namespace {

constexpr std::size_t kMaximumEncodedPresetBytes = 256u * 1024u * 1024u;

bool readStream(PkStream *stream, PkByteArray &result)
{
    result = PkByteArray();
    if (!stream || !stream->isReadable()) {
        return false;
    }

    std::vector<std::uint8_t> bytes;
    char chunk[64 * 1024];
    while (true) {
        const PkStream::pk_int64 count = stream->read(chunk, sizeof(chunk));
        if (count < 0) {
            return false;
        }
        if (count == 0) {
            break;
        }
        if (static_cast<std::size_t>(count) >
            kMaximumEncodedPresetBytes - bytes.size()) {
            return false;
        }
        bytes.insert(bytes.end(), chunk, chunk + count);
    }
    if (bytes.empty() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    result = PkByteArray(bytes);
    return true;
}

bool writeAll(PkStream *stream, const PkByteArray &data)
{
    if (!stream || !stream->isWritable() || data.isEmpty()) {
        return false;
    }
    PkStream::pk_int64 offset = 0;
    while (offset < data.size()) {
        const PkStream::pk_int64 count =
            stream->write(data.constData() + offset, data.size() - offset);
        if (count <= 0) {
            return false;
        }
        offset += count;
    }
    return true;
}

void replaceAll(std::string &text, const std::string &before, const std::string &after)
{
    if (before.empty()) {
        return;
    }
    std::size_t offset = 0;
    while ((offset = text.find(before, offset)) != std::string::npos) {
        text.replace(offset, before.size(), after);
        offset += after.size();
    }
}

PkString replaceUtf8(const PkString &text, const std::string &before, const std::string &after)
{
    std::string utf8 = text.PkToUtf8();
    replaceAll(utf8, before, after);
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

bool isBase64Character(unsigned char value)
{
    return (value >= 'A' && value <= 'Z') ||
        (value >= 'a' && value <= 'z') ||
        (value >= '0' && value <= '9') || value == '+' || value == '/' || value == '=';
}

void removeInvalidPatternMd5(std::string &xml)
{
    constexpr const char *parameterName = "name=\"Texture/Pattern/PatternMD5\"";
    constexpr const char *cdataStartText = "<![CDATA[";
    constexpr const char *cdataEndText = "]]>";
    constexpr const char *elementEndText = "</param>";

    std::size_t offset = 0;
    while ((offset = xml.find("<param", offset)) != std::string::npos) {
        const std::size_t openingEnd = xml.find('>', offset);
        if (openingEnd == std::string::npos) {
            return;
        }
        const std::string opening = xml.substr(offset, openingEnd - offset + 1);
        if (opening.find(parameterName) == std::string::npos) {
            offset = openingEnd + 1;
            continue;
        }

        const std::size_t elementEnd = xml.find(elementEndText, openingEnd + 1);
        const std::size_t cdataStart = xml.find(cdataStartText, openingEnd + 1);
        if (elementEnd == std::string::npos || cdataStart == std::string::npos ||
            cdataStart >= elementEnd) {
            offset = openingEnd + 1;
            continue;
        }
        const std::size_t valueStart = cdataStart + std::strlen(cdataStartText);
        const std::size_t cdataEnd = xml.find(cdataEndText, valueStart);
        if (cdataEnd == std::string::npos || cdataEnd > elementEnd) {
            offset = openingEnd + 1;
            continue;
        }
        const bool invalid = std::any_of(xml.begin() + valueStart, xml.begin() + cdataEnd,
                                         [](unsigned char value) {
                                             return !isBase64Character(value);
                                         });
        if (invalid) {
            xml.erase(offset, elementEnd + std::strlen(elementEndText) - offset);
        } else {
            offset = elementEnd + std::strlen(elementEndText);
        }
    }
}

bool isValidBase64(const PkString &encoded)
{
    const std::string text = encoded.PkToUtf8();
    std::size_t significant = 0;
    std::size_t padding = 0;
    bool sawPadding = false;
    for (unsigned char value : text) {
        if (std::isspace(value)) {
            continue;
        }
        if (!isBase64Character(value)) {
            return false;
        }
        if (value == '=') {
            sawPadding = true;
            ++padding;
        } else if (sawPadding) {
            return false;
        }
        ++significant;
    }
    return significant > 0 && significant % 4 == 0 && padding <= 2;
}

PkString encodeBase64(const PkByteArray &data)
{
    if (data.isEmpty() || data.size() > std::numeric_limits<int>::max() - 2) {
        return PkString();
    }

    const int remainder = data.size() % 3;
    if (remainder == 0) {
        return pkToBase64(data);
    }

    PkByteArray padded = data;
    const int padding = 3 - remainder;
    padded.resize(data.size() + padding);
    std::string encoded = pkToBase64(padded).PkToUtf8();
    if (encoded.size() < static_cast<std::size_t>(padding)) {
        return PkString();
    }
    std::fill(encoded.end() - padding, encoded.end(), '=');
    return PkString::PkFromUtf8(encoded.data(), static_cast<int>(encoded.size()));
}

} // namespace

struct KisPaintOpPreset::Private {

    struct UpdateListener : public KisPaintOpSettings::UpdateListener {
        UpdateListener(KisPaintOpPreset *parentPreset)
            : m_parentPreset(parentPreset)
        {
        }

        void setDirty(bool value) override {
            m_parentPreset->setDirty(value);
        }

        bool isDirty() const override {
            return m_parentPreset->isDirty();
        }

        void notifySettingsChanged() override {
            KisPaintOpPresetUpdateProxy* proxy = m_parentPreset->updateProxyNoCreate();
            if (proxy) {
                proxy->notifySettingsChanged();
            }
        }

    private:
        KisPaintOpPreset *m_parentPreset;
    };

public:
    Private(KisPaintOpPreset *q)
        : settingsUpdateListener(new UpdateListener(q)),
          version("5.0")
    {
    }

    KisPaintOpSettingsSP settings {0};
    PkScopedPointer<KisPaintOpPresetUpdateProxy> updateProxy;
    KisPaintOpSettings::UpdateListenerSP settingsUpdateListener;
    PkString version;
    PkList<KoResourceLoadResult> sideLoadedResources;
};


KisPaintOpPreset::KisPaintOpPreset()
    : KoResource(PkString())
    , d(new Private(this))
{
}

KisPaintOpPreset::KisPaintOpPreset(const PkString & fileName)
    : KoResource(fileName)
    , d(new Private(this))
{
    setName(replaceUtf8(name(), "_", " "));
}

KisPaintOpPreset::~KisPaintOpPreset()
{
    delete d;
}

KisPaintOpPreset::KisPaintOpPreset(const KisPaintOpPreset &rhs)
    : KoResource(rhs)
    , d(new Private(this))
{
    if (rhs.settings()) {
        setSettings(rhs.settings()); // the settings are cloned inside!
    }
    KIS_SAFE_ASSERT_RECOVER_NOOP(isDirty() == rhs.isDirty());
    // only valid if we could clone the settings
    setValid(rhs.settings());

    setName(rhs.name());
    setImage(rhs.image());
}

KoResourceSP KisPaintOpPreset::clone() const
{
    return KoResourceSP(new KisPaintOpPreset(*this));
}

void KisPaintOpPreset::setPaintOp(const KoID & paintOp)
{
    Q_ASSERT(d->settings);
    d->settings->setProperty("paintop", paintOp.id());
}

KoID KisPaintOpPreset::paintOp() const
{
    Q_ASSERT(d->settings);
    return KoID(d->settings->getString("paintop"));
}

PkString KisPaintOpPreset::name() const
{
    return replaceUtf8(KoResource::name(), "_", " ");
}

void KisPaintOpPreset::setSettings(KisPaintOpSettingsSP settings)
{
    Q_ASSERT(settings);
    Q_ASSERT(!settings->getString("paintop", PkString()).isEmpty());

    KisDirtyStateSaver<KisPaintOpPreset*> dirtyStateSaver(this);

    if (d->settings) {
        d->settings->setUpdateListener(KisPaintOpSettings::UpdateListenerWSP());
        d->settings = 0;
    }

    if (settings) {
        d->settings = settings->clone();
        d->settings->setUpdateListener(d->settingsUpdateListener);
    }

    if (d->updateProxy) {
        d->updateProxy->notifyUniformPropertiesChanged();
        d->updateProxy->notifySettingsChanged();
    }
    setValid(true);
}

KisPaintOpSettingsSP KisPaintOpPreset::settings() const
{
    Q_ASSERT(d->settings);
    Q_ASSERT(!d->settings->getString("paintop", PkString()).isEmpty());

    return d->settings;
}

bool KisPaintOpPreset::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    if (!resourcesInterface) {
        return false;
    }

    PkByteArray encoded;
    if (!readStream(dev, encoded)) {
        return false;
    }
    KisResourceThumbnailCodec::PngPayload payload;
    if (!KisResourceThumbnailCodec::decodePng(encoded, payload) || payload.image.isNull()) {
        dbgImage << "Fail to decode PNG";
        return false;
    }

    d->version = payload.text.value(PkString("version"));
    PkString preset = payload.text.value(PkString("preset"));

    if (!(d->version == "2.2" || d->version == "5.0") || preset.isEmpty()) {
        return false;
    }

    std::string presetXml = preset.PkToUtf8();
    replaceAll(presetXml, "<curve><![CDATA[", "<curve>");
    replaceAll(presetXml, "]]></curve>", "</curve>");
    removeInvalidPatternMd5(presetXml);
    preset = PkString::PkFromUtf8(presetXml.data(), static_cast<int>(presetXml.size()));

    PkXmlDocument doc;
    if (!doc.setContent(preset)) {
        return false;
    }

    PkXmlElement root = doc.documentElement();
    if (root.isNull()) {
        return false;
    }

    PkList<KoResourceLoadResult> sideLoadedResources;

    if (d->version == "5.0") {
        // Load any embedded resources
        PkXmlElement e = root.firstChildElement("resources");
        if (!e.isNull()) {
            for (e = e.firstChildElement("resource"); !e.isNull(); e = e.nextSiblingElement("resource")) {
                PkString name = e.attribute("name");
                PkString filename = e.attribute("filename");
                PkString resourceType = e.attribute("type");
                PkString md5sum = e.attribute("md5sum");

                KoResourceSP existingResource = resourcesInterface
                        ->source(resourceType)
                        .exactMatch(md5sum, filename, name);

                if (existingResource) {
                    continue;
                }

                const PkString base64 = e.text();
                if (!isValidBase64(base64)) {
                    return false;
                }
                const PkByteArray data = pkFromBase64(base64);
                if (data.isEmpty()) {
                    return false;
                }
                sideLoadedResources.append(
                    KoEmbeddedResource(
                        KoResourceSignature(resourceType, md5sum, filename, name), data));
            }
        }
    }

    fromXML(root, resourcesInterface);

    if (!d->settings || !valid() || !d->settings->isValid()) {
        setValid(false);
        return false;
    }

    d->sideLoadedResources = sideLoadedResources;
    setValid(true);
    setImage(payload.image);

    updateLinkedResourcesMetaData();

    return true;
}

bool KisPaintOpPreset::toXML(PkXmlDocument& doc, PkXmlElement& elt) const
{
    if (!d->settings) {
        return false;
    }
    PkString paintopid = d->settings->getString("paintop", PkString());

    elt.setAttribute("paintopid", paintopid);
    elt.setAttribute("name", name());


    PkList<KoResourceLoadResult> linkedResources = this->linkedResources(resourcesInterface());

    elt.setAttribute("embedded_resources", KisDomUtils::toString(linkedResources.count()));

    if (!linkedResources.isEmpty()) {
        PkXmlElement resourcesElement = doc.createElement("resources");
        elt.appendChild(resourcesElement);
        for (const KoResourceLoadResult &linkedResource : linkedResources) {
            if (linkedResource.type() == KoResourceLoadResult::EmbeddedResource) {
                warnKrita << "KisPaintOpPreset::toXML got an unexpected embedded resource";
                return false;
            }

            KoResourceSP resource = linkedResource.resource();

            if (!resource) {
                warnKrita << "KisPaintOpPreset::toXML couldn't fetch a linked resource";
                return false;
            }

            if (!resource->isSerializable()) {
                warnKrita << "KisPaintOpPreset::toXML cannot embed a non-serializable resource";
                return false;
            }

            PkMemoryStream buffer;
            if (!buffer.open(PkStream::WriteOnly)) {
                return false;
            }
            KisResourceModel model(resource->resourceType().first);
            if (!model.exportResource(resource, &buffer) || buffer.size() <= 0 ||
                buffer.size() > std::numeric_limits<int>::max()) {
                return false;
            }
            const PkByteArray data(buffer.data(), static_cast<int>(buffer.size()));
            const PkString encoded = encodeBase64(data);
            if (encoded.isEmpty() || pkFromBase64(encoded) != data) {
                return false;
            }
            PkXmlCDATASection text = doc.createCDATASection(encoded);
            PkXmlElement e = doc.createElement("resource");
            e.setAttribute("type", resource->resourceType().first);
            e.setAttribute("md5sum", resource->md5Sum());
            e.setAttribute("name", resource->name());
            e.setAttribute("filename", resource->filename());
            e.appendChild(text);
            resourcesElement.appendChild(e);
        }
    }

    // sanitize the settings
    bool hasTexture = d->settings->getBool("Texture/Pattern/Enabled");
    if (!hasTexture) {
        for (const PkString & key : d->settings->getProperties().keys()) {
            if (key.startsWith("Texture") && key != "Texture/Pattern/Enabled") {
                d->settings->removeProperty(key);
            }
        }
    }

    d->settings->toXML(doc, elt);
    return true;
}

void KisPaintOpPreset::fromXML(const PkXmlElement& presetElt, KisResourcesInterfaceSP resourcesInterface)
{
    setName(presetElt.attribute("name"));
    PkString paintopid = presetElt.attribute("paintopid");

    if (!metadata().contains("paintopid")) {
        addMetaData("paintopid", paintopid);
    }

    if (paintopid.isEmpty()) {
        dbgImage << "No paintopid attribute";
        setValid(false);
        return;
    }

    if (KisPaintOpRegistry::instance()->get(paintopid) == 0) {
        dbgImage << "No paintop " << paintopid;
        setValid(false);
        return;
    }

    KoID id(paintopid, PkString());

    KisPaintOpSettingsSP settings = KisPaintOpRegistry::instance()->createSettings(id, resourcesInterface);
    if (!settings) {
        setValid(false);
        warnKrita << "Could not load settings for preset" << paintopid;
        return;
    }

    settings->fromXML(presetElt);

    // sanitize the settings
    bool hasTexture = settings->getBool("Texture/Pattern/Enabled");
    if (!hasTexture) {
        for (const PkString & key : settings->getProperties().keys()) {
            if (key.startsWith("Texture") && key != "Texture/Pattern/Enabled") {
                settings->removeProperty(key);
            }
        }
    }
    setSettings(settings);

}

bool KisPaintOpPreset::saveToDevice(PkStream* dev) const
{
    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("Preset");

    /**
     * HACK ALERT: We update the version of the resource format on
     * the first save operation, even though there is no guarantee
     * that it was "save" operation, but not "export" operation.
     *
     * The only point it affects now is whether we need to check
     * for the presence of the linkedResources() in
     * updateLinkedResourcesMetaData(). The new version of the
     * preset format ("5.0") has all the linked resources embedded
     * outside KisPaintOpSettings, which are automatically
     * loaded on the resource activation. We we shouldn't
     * add them into metaData()["dependent_resources_filenames"].
     */
    d->version = "5.0";

    const_cast<KisPaintOpPreset*>(this)->updateLinkedResourcesMetaData();

    if (!toXML(doc, root)) {
        return false;
    }
    doc.appendChild(root);

    const PkString preset = doc.toString();
    if (preset.isEmpty()) {
        return false;
    }
    PkMap<PkString, PkString> text;
    text.insert(PkString("version"), d->version);
    text.insert(PkString("preset"), preset);

    const PkImage preview = image().isNull()
        ? PkImage(1, 1, PkImage::Format_RGB32) : image();
    const PkByteArray encoded = KisResourceThumbnailCodec::encodePng(preview, text);
    return writeAll(dev, encoded);
}

void KisPaintOpPreset::updateLinkedResourcesMetaData()
{
    /**
     * The new preset format embeds all the linked resources outside
     * KisPaintOpSettings and loads them on activation, therefore we
     * shouldn't add them into "dependent_resources_filenames".
     */

    if (d->version == "2.2") {
        KisResourcesInterfaceSP fakeResourcesInterface(new KisLocalStrokeResources());
        PkList<KoResourceLoadResult> dependentResources = this->linkedResources(fakeResourcesInterface);

        PkStringList resourceFileNames;

        for (const KoResourceLoadResult &resource : dependentResources) {
            const PkString filename = resource.signature().filename;

            if (!filename.isEmpty()) {
                resourceFileNames.append(filename);
            }
        }

        KritaUtils::makeContainerUnique(resourceFileNames);

        if (!resourceFileNames.isEmpty()) {
            addMetaData("dependent_resources_filenames", resourceFileNames);
        }
    } else {
        addMetaData("dependent_resources_filenames", PkStringList());
    }
}

PkPointer<KisPaintOpPresetUpdateProxy> KisPaintOpPreset::updateProxy() const
{
    if (!d->updateProxy) {
        d->updateProxy.reset(new KisPaintOpPresetUpdateProxy());
    }
    return d->updateProxy.data();
}

PkPointer<KisPaintOpPresetUpdateProxy> KisPaintOpPreset::updateProxyNoCreate() const
{
    return d->updateProxy.data();
}

PkList<KisUniformPaintOpPropertySP> KisPaintOpPreset::uniformProperties()
{
    /// we pass a shared pointer to settings explicitly,
    /// because the settings will not be able to wrap
    /// itself into a shared pointer
    return d->settings->uniformProperties(d->settings, updateProxy());
}

bool KisPaintOpPreset::hasMaskingPreset() const
{
    return d->settings && d->settings->hasMaskingSettings();
}

KisPaintOpPresetSP KisPaintOpPreset::createMaskingPreset() const
{
    KisPaintOpPresetSP result;

    if (d->settings && d->settings->hasMaskingSettings()) {
        result.reset(new KisPaintOpPreset());
        result->setSettings(d->settings->createMaskingSettings());
        if (!result->valid()) {
            result.clear();
        }
    }

    return result;
}

KisResourcesInterfaceSP KisPaintOpPreset::resourcesInterface() const
{
    return d->settings ? d->settings->resourcesInterface() : nullptr;
}

void KisPaintOpPreset::setResourcesInterface(KisResourcesInterfaceSP resourcesInterface)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(d->settings);
    d->settings->setResourcesInterface(resourcesInterface);
}

KoCanvasResourcesInterfaceSP KisPaintOpPreset::canvasResourcesInterface() const
{
    return d->settings ? d->settings->canvasResourcesInterface() : nullptr;
}

void KisPaintOpPreset::setCanvasResourcesInterface(KoCanvasResourcesInterfaceSP canvasResourcesInterface)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(d->settings);
    d->settings->setCanvasResourcesInterface(canvasResourcesInterface);
}

bool KisPaintOpPreset::hasLocalResourcesSnapshot() const
{
    return KisRequiredResourcesOperators::hasLocalResourcesSnapshot(this);
}

KisPaintOpPresetSP KisPaintOpPreset::cloneWithResourcesSnapshot(KisResourcesInterfaceSP globalResourcesInterface, KoCanvasResourcesInterfaceSP canvasResourcesInterface, KoResourceCacheInterfaceSP cacheInterface) const
{
    KisPaintOpPresetSP result =
            KisRequiredResourcesOperators::cloneWithResourcesSnapshot<KisPaintOpPresetSP>(this, globalResourcesInterface);

    const PkList<int> canvasResources = result->requiredCanvasResources();
    if (!canvasResources.isEmpty()) {
        KoLocalStrokeCanvasResourcesSP storage(new KoLocalStrokeCanvasResources());
        for (int key : canvasResources) {
            storage->storeResource(key, canvasResourcesInterface->resource(key));
        }
        result->setCanvasResourcesInterface(storage);
    }

    if (cacheInterface) {
        result->setResourceCacheInterface(cacheInterface);
    } else if (!canvasResources.isEmpty()) {
        /**
         * If the preset depends on any canvas resources, then we don't trust
         * the caches that are stored inside. Instead we just reset them. If the
         * preset is independent of the canvas resources, then its caches are,
         * most probably valid and we can reuse them.
         */
        result->setResourceCacheInterface(nullptr);
    }

    return result;
}

PkList<KoResourceLoadResult> KisPaintOpPreset::linkedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    PkList<KoResourceLoadResult> resources;

    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(d->settings, resources);

    KisPaintOpFactory* f = KisPaintOpRegistry::instance()->value(paintOp().id());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(f, resources);
    resources << f->prepareLinkedResources(d->settings, globalResourcesInterface);

    if (hasMaskingPreset()) {
        KisPaintOpPresetSP maskingPreset = createMaskingPreset();
        Q_ASSERT(maskingPreset);

        KisPaintOpFactory* f = KisPaintOpRegistry::instance()->value(maskingPreset->paintOp().id());
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(f, resources);
        resources << f->prepareLinkedResources(maskingPreset->settings(), globalResourcesInterface);

    }

    return resources;
}

PkList<KoResourceLoadResult> KisPaintOpPreset::embeddedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    PkList<KoResourceLoadResult> resources;

    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(d->settings, resources);

    KisPaintOpFactory* f = KisPaintOpRegistry::instance()->value(paintOp().id());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(f, resources);
    resources << f->prepareEmbeddedResources(d->settings, globalResourcesInterface);

    if (hasMaskingPreset()) {
        KisPaintOpPresetSP maskingPreset = createMaskingPreset();
        Q_ASSERT(maskingPreset);
        KisPaintOpFactory* f = KisPaintOpRegistry::instance()->value(maskingPreset->paintOp().id());
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(f, resources);
        resources << f->prepareEmbeddedResources(maskingPreset->settings(), globalResourcesInterface);

    }

    return resources;
}

PkList<KoResourceLoadResult> KisPaintOpPreset::sideLoadedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    PkList<KoResourceLoadResult> resources;

    for (const KoResourceLoadResult &resource : d->sideLoadedResources) {
        KoResourceSignature sig = resource.signature();

        /**
         * Do not load the existing resources. There is no use for it.
         */
        if (!globalResourcesInterface->source(sig.type)
                .exactMatch(sig.md5sum, sig.filename, sig.name)) {

            resources << resource;
        }
    }

    return resources;
}

void KisPaintOpPreset::clearSideLoadedResources()
{
    d->sideLoadedResources.clear();
}

PkList<int> KisPaintOpPreset::requiredCanvasResources() const
{
    return d->settings ? d->settings->requiredCanvasResources() : PkList<int>();
}

void KisPaintOpPreset::setResourceCacheInterface(KoResourceCacheInterfaceSP cacheInterface)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(d->settings);
    d->settings->setResourceCacheInterface(cacheInterface);
}

KoResourceCacheInterfaceSP KisPaintOpPreset::resourceCacheInterface() const
{
    return d->settings ? d->settings->resourceCacheInterface() : KoResourceCacheInterfaceSP();
}

void KisPaintOpPreset::regenerateResourceCache(KoResourceCacheInterfaceSP cacheInterface)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(d->settings);

    d->settings->regenerateResourceCache(cacheInterface);
    cacheInterface->setRelatedResourceCookie(d->settings->sanityVersionCookie());
}

bool KisPaintOpPreset::sanityCheckResourceCacheIsValid(KoResourceCacheInterfaceSP cacheInterface) const
{
    return d->settings->sanityVersionCookie() == cacheInterface->relatedResourceCookie();
}

KisPaintOpPreset::UpdatedPostponer::UpdatedPostponer(KisPaintOpPresetSP preset)
    : m_updateProxy(preset->updateProxyNoCreate())
{
    if (m_updateProxy) {
        m_updateProxy->postponeSettingsChanges();
    }
}

KisPaintOpPreset::UpdatedPostponer::~UpdatedPostponer()
{
    if (m_updateProxy) {
        m_updateProxy->unpostponeSettingsChanges();
    }
}
