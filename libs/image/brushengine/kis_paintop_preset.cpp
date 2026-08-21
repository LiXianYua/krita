/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2009 Sven Langkamp <sven.langkamp@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <brushengine/kis_paintop_preset.h>

// ===========================================================================
// [GAP] kis_paintop_preset.cpp 阻塞登记（S-06 Task 4）
//
// 本文件不进薄壳，仅剥可机械映射类型；下列阻塞点保留 Qt 原样并标注 [GAP]：
//   * PNG 编解码 QImageReader / QImageWriter / QImage —— 依赖 S-03-e
//     （pk/image PNG codec，未交付）
//   * base64 内存缓冲：PkByteArray 已有（pk/variant/PkAuxTypes.h）但无
//     fromBase64 / toBase64，且无 PkBuffer 等价物 —— loadFromDevice 的嵌入
//     资源解码、toXML 的导出都压在它上面
//   * 正则 QRegularExpression —— 无对应 Pk 模块
//   * PkString::replace 未实现（name() 的空格归一、loadFromDevice 的 CDATA
//     清理都压在它上面）
//   * settings->fromXML(...) 依赖 kis_properties_configuration 的 Qt 兼容层
//     （本文件进薄壳时由 compat 宏把 QDomElement→PkXmlElement 对上）
// 关闭条件：上述依赖交付后，把 [GAP] 处换成 Pk 等价物，并将本文件加入薄壳
// SHELL_SOURCES。当前状态：签名已与剥离后的头文件对齐，可机械映射类型已转 Pk，
// 其余留 Qt + [GAP] 标记，不参与薄壳构建。
// ===========================================================================
#include <QImage>
#include <QImageWriter>
#include <QImageReader>
#include <QBuffer>

#include <PkString.h>
#include <PkStringList.h>
#include <PkList.h>
#include <PkScopedPointer.h>
#include <PkPointer.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkXmlCDATASection.h>
#include <PkStream.h>

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

struct Q_DECL_HIDDEN KisPaintOpPreset::Private {

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
    // [GAP] PkString::replace 未实现
    setName(name().replace("_", " "));
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
    // [GAP] PkString::replace 未实现
    return KoResource::name().replace("_", " ");
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
    // [GAP] PNG 解码（QImageReader / QImage）+ preset 字符串提取 —— 依赖 S-03-e
    QImageReader reader(dev, "PNG");

    d->version = reader.text("version");
    QString preset = reader.text("preset");

    if (!(d->version == "2.2" || d->version == "5.0")) {
        return false;
    }

    QImage img;
    if (!reader.read(&img)) {
        dbgImage << "Fail to decode PNG";
        return false;
    }

    // [GAP] preset 字符串清洗：PkString::replace 未实现 + QRegularExpression
    //       无对应 Pk 模块
    //Workaround for broken presets
    //Presets was saved with nested cdata section
    preset.replace("<curve><![CDATA[", "<curve>");
    preset.replace("]]></curve>", "</curve>");
    //Presets with non-base64 pattern md5
    QRegularExpressionMatch patternMd5 = QRegularExpression("<param (?:type=\"string\" )?name=\"Texture/Pattern/PatternMD5\"(?: type=\"string\")?><!\\[CDATA\\[(.+?)\\]\\]></param>").match(preset);
    if (patternMd5.hasMatch() && patternMd5.captured(1).contains(QRegularExpression("[^a-zA-Z0-9+/=]"))) {
        preset.replace(patternMd5.captured(0), "");
    }

    PkXmlDocument doc;
    if (!doc.setContent(preset)) {   // preset 仍为 QString（[GAP]），关闭后换 PkString
        return false;
    }

    PkXmlElement root = doc.documentElement();

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

                // [GAP] base64 内存缓冲：PkByteArray 无 fromBase64、无 PkBuffer 等价物
                QByteArray ba = QByteArray::fromBase64(e.text().toLatin1());
                QBuffer buf(&ba);
                buf.open(QBuffer::ReadOnly);

                d->sideLoadedResources.append(
                            KoEmbeddedResource(
                                KoResourceSignature(resourceType, md5sum, filename, name),
                                ba));
            }
        }
    }

    fromXML(root, resourcesInterface);

    if (!d->settings) {
        return false;
    }

    setValid(d->settings->isValid());

    // [GAP] 图片去元数据：QImage 纹理剥离 —— 依赖 S-03-e
    if (!img.textKeys().isEmpty()) {
        QImage strippedImage(img.size(), img.format());
        memcpy(strippedImage.bits(), img.bits(), img.sizeInBytes());

        if (img.format() == QImage::Format_Indexed8) {
            strippedImage.setColorTable(img.colorTable());
        }

        setImage(strippedImage);
    } else {
        setImage(img);
    }

    updateLinkedResourcesMetaData();

    return true;
}

void KisPaintOpPreset::toXML(PkXmlDocument& doc, PkXmlElement& elt) const
{
    PkString paintopid = d->settings->getString("paintop", PkString());

    elt.setAttribute("paintopid", paintopid);
    elt.setAttribute("name", name());


    PkList<KoResourceLoadResult> linkedResources = this->linkedResources(resourcesInterface());

    elt.setAttribute("embedded_resources", KisDomUtils::toString(linkedResources.count()));

    if (!linkedResources.isEmpty()) {
        PkXmlElement resourcesElement = doc.createElement("resources");
        elt.appendChild(resourcesElement);
        for (const KoResourceLoadResult &linkedResource : linkedResources) {
            // we have requested linked resources, how can it be an embedded one?
            KIS_SAFE_ASSERT_RECOVER(linkedResource.type() != KoResourceLoadResult::EmbeddedResource) { continue; }

            KoResourceSP resource = linkedResource.resource();

            if (!resource) {
                qWarning() << "WARNING: KisPaintOpPreset::toXML couldn't fetch a linked resource" << linkedResource.signature();
                continue;
            }

            //KIS_SAFE_ASSERT_RECOVER_NOOP(resource->isSerializable() && "embedding non-serializable resources is not yet implemented");
            if (!resource->isSerializable()) {
                qWarning() << "embedding non-serializable resources is not yet implemented. Resource: " << filename() << name()
                           << "cannot embed" << resource->filename() << resource->name() << resource->resourceType().first << resource->resourceType().second;
                continue;
            }

            // [GAP] base64 导出：QBuffer + PkByteArray::toBase64 + QString::fromLatin1
            //       未实现（PkByteArray 无 toBase64、无 PkBuffer 等价物）
            QBuffer buf;
            buf.open(QBuffer::WriteOnly);
            KisResourceModel model(resource->resourceType().first);
            bool r = model.exportResource(resource, &buf);
            buf.close();
            if (r) {
                PkXmlCDATASection text = doc.createCDATASection(QString::fromLatin1(buf.data().toBase64()));
                PkXmlElement e = doc.createElement("resource");
                e.setAttribute("type", resource->resourceType().first);
                e.setAttribute("md5sum", resource->md5Sum());
                e.setAttribute("name", resource->name());
                e.setAttribute("filename", resource->filename());
                e.appendChild(text);
                resourcesElement.appendChild(e);

            }
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
        // [GAP] QDebug<<PkString 流运算符缺失
        dbgImage << "No paintop " << paintopid;
        setValid(false);
        return;
    }

    KoID id(paintopid, PkString());

    KisPaintOpSettingsSP settings = KisPaintOpRegistry::instance()->createSettings(id, resourcesInterface);
    if (!settings) {
        setValid(false);
        // [GAP] QDebug<<PkString 流运算符缺失
        warnKrita << "Could not load settings for preset" << paintopid;
        return;
    }

    // settings->fromXML(...) 依赖 kis_properties_configuration 的兼容层
    //（进薄壳时 compat 宏把 QDomElement→PkXmlElement 对上；见文件头 [GAP] 登记）
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
    // [GAP] PNG 编码 QImageWriter —— 依赖 S-03-e
    QImageWriter writer(dev, "PNG");

    PkXmlDocument doc;
    PkXmlElement root = doc.createElement("Preset");

    toXML(doc, root);

    doc.appendChild(root);

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

    writer.setText("version", d->version);
    writer.setText("preset", doc.toString());

    // [GAP] QImage 纹理 —— 依赖 S-03-e
    QImage img;

    if (image().isNull()) {
        img = QImage(1, 1, QImage::Format_RGB32);
    } else {
        img = image();
    }

    return writer.write(img);
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
    resources << f->prepareLinkedResources(d->settings, globalResourcesInterface).toVector();

    if (hasMaskingPreset()) {
        KisPaintOpPresetSP maskingPreset = createMaskingPreset();
        Q_ASSERT(maskingPreset);

        KisPaintOpFactory* f = KisPaintOpRegistry::instance()->value(maskingPreset->paintOp().id());
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(f, resources);
        resources << f->prepareLinkedResources(maskingPreset->settings(), globalResourcesInterface).toVector();

    }

    return resources;
}

PkList<KoResourceLoadResult> KisPaintOpPreset::embeddedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    PkList<KoResourceLoadResult> resources;

    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(d->settings, resources);

    KisPaintOpFactory* f = KisPaintOpRegistry::instance()->value(paintOp().id());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(f, resources);
    resources << f->prepareEmbeddedResources(d->settings, globalResourcesInterface).toVector();

    if (hasMaskingPreset()) {
        KisPaintOpPresetSP maskingPreset = createMaskingPreset();
        Q_ASSERT(maskingPreset);
        KisPaintOpFactory* f = KisPaintOpRegistry::instance()->value(maskingPreset->paintOp().id());
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(f, resources);
        resources << f->prepareEmbeddedResources(maskingPreset->settings(), globalResourcesInterface).toVector();

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
