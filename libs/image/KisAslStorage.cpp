/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisAslStorage.h"
#include <KisResourceStorage.h>
#include <kis_psd_layer_style.h>

#include <PkString.h>
#include <PkSharedPointer.h>
#include <PkHash.h>
#include <PkVector.h>
#include <PkScopedPointer.h>
#include <PkDateTime.h>
#include <PkHashIterator.h>
#include <PkVectorIterator.h>
#include <sys/stat.h>
#include <KisStaticInitializer.h>
#include <KisGlobalResourcesInterface.h>

// 文件元信息在 Pk 侧无等价物，这里用 PkString + POSIX stat 原地适配。
// 语义对齐：lastModified()（文件不存在返回无效时间）/
// baseName()（去掉目录前缀与最后一个扩展名）/ lastIndexOf()。
static int pkLastIndexOf(const PkString &s, const PkString &sub)
{
    if (sub.isEmpty()) return -1;
    const int slen = s.size();
    const int sublen = sub.size();
    if (sublen > slen) return -1;
    for (int i = slen - sublen; i >= 0; --i) {
        if (s.mid(i, sublen) == sub) return i;
    }
    return -1;
}

static PkString pkBaseName(const PkString &path)
{
    PkString s = path;
    const int slash = pkLastIndexOf(s, "/");
    const int backslash = pkLastIndexOf(s, "\\");
    const int start = slash > backslash ? slash : backslash;
    if (start >= 0) s = s.mid(start + 1);
    const int dot = pkLastIndexOf(s, ".");
    if (dot > 0) s = s.left(dot);
    return s;
}

static PkDateTime pkFileLastModified(const PkString &path)
{
    // 壳侧无文件元信息类型，用 POSIX stat 取 mtime（不存在返回无效时间）。
    struct stat st;
    if (::stat(path.PkToUtf8().c_str(), &st) != 0) {
        return PkDateTime();
    }
    return PkDateTime::fromSecsSinceEpoch(static_cast<std::int64_t>(st.st_mtime));
}

KIS_DECLARE_STATIC_INITIALIZER {
    KisStoragePluginRegistry::instance()->addStoragePluginFactory(KisResourceStorage::StorageType::AdobeStyleLibrary, new KisStoragePluginFactory<KisAslStorage>());
}

class AslTagIterator : public KisResourceStorage::TagIterator
{
public:

    AslTagIterator(const PkString &location, const PkString &resourceType)
        : m_location(location)
        , m_resourceType(resourceType)
    {}

    bool hasNext() const override {return false;}
    void next() override {}

    KisTagSP tag() const override { return nullptr; }

private:

    PkString m_location;
    PkString m_resourceType;

};

class AslIterator : public KisResourceStorage::ResourceIterator
{

private:

    PkString m_filename;
    PkSharedPointer<KisAslLayerStyleSerializer> m_aslSerializer;
    bool m_isLoaded;
    PkHash<PkString, KoPatternSP> m_patterns;
    PkVector<KisPSDLayerStyleSP> m_styles;
    PkScopedPointer<PkHashIterator<PkString, KoPatternSP>> m_patternsIterator;
    PkScopedPointer<PkVectorIterator<KisPSDLayerStyleSP>> m_stylesIterator;
    PkString m_currentType;
    KoResourceSP m_currentResource;
    PkString m_currentUuid;
    PkString m_resourceType;

public:

    AslIterator(PkSharedPointer<KisAslLayerStyleSerializer> aslSerializer, const PkString& filename, const PkString& resourceType)
        : m_filename(filename)
        , m_aslSerializer(aslSerializer)
        , m_isLoaded(false)
        , m_resourceType(resourceType)
    {
    }

    bool hasNext() const override
    {
        if (!m_isLoaded && (m_resourceType == ResourceType::Patterns || m_resourceType == ResourceType::LayerStyles)) {
            if (!m_aslSerializer->isInitialized()) {
                m_aslSerializer->readFromFile(m_filename);
            }

            const_cast<AslIterator*>(this)->m_isLoaded = true;
            const_cast<AslIterator*>(this)->m_patterns = m_aslSerializer->patterns();
            const_cast<AslIterator*>(this)->m_styles = m_aslSerializer->styles();

            const_cast<AslIterator*>(this)->m_patternsIterator.reset(new PkHashIterator<PkString, KoPatternSP>(m_patterns));
            const_cast<AslIterator*>(this)->m_stylesIterator.reset(new PkVectorIterator<KisPSDLayerStyleSP>(m_styles));
        }
        if (!m_aslSerializer->isValid()) {
            return false;
        }

        if (m_resourceType == ResourceType::Patterns) {
            return m_patternsIterator->hasNext();
        } else if (m_resourceType == ResourceType::LayerStyles) {
            return m_stylesIterator->hasNext();
        }
        return false;
    }
    void next() override
    {
        if (m_resourceType == ResourceType::Patterns) {
            if (m_patternsIterator->hasNext()) {
                m_currentType = ResourceType::Patterns;
                m_patternsIterator->next();
                KoPatternSP currentPattern = m_patternsIterator->value();
                m_currentResource = currentPattern;
                KIS_ASSERT(currentPattern);
                m_currentUuid = currentPattern->filename();
            }
        } else if (m_resourceType == ResourceType::LayerStyles) {
            if (m_stylesIterator->hasNext()) {
                m_currentType = ResourceType::LayerStyles;
                KisPSDLayerStyleSP currentLayerStyle = m_stylesIterator->next();
                m_currentResource = currentLayerStyle;
                KIS_ASSERT(currentLayerStyle);
                m_currentUuid = currentLayerStyle->filename();
            }
        }
    }

    PkString url() const override
    {
        if (m_currentResource.isNull()) {
            return PkString();
        }
        return m_currentUuid;
    }

    PkString type() const override
    {
        return m_currentResource.isNull() ? PkString() : m_currentType;
    }

    PkDateTime lastModified() const override {
        return pkFileLastModified(m_filename);
    }


    /// This only loads the resource when called (but not in case of asl...)
    KoResourceSP resourceImpl() const override
    {
        return m_currentResource;
    }
};

KisAslStorage::KisAslStorage(const PkString &location)
    : KisStoragePlugin(location)
    , m_aslSerializer(new KisAslLayerStyleSerializer())
{
}

KisAslStorage::~KisAslStorage()
{

}

KisResourceStorage::ResourceItem KisAslStorage::resourceItem(const PkString &url)
{
    KisResourceStorage::ResourceItem item;
    item.url = url;
    item.folder = location();
    item.resourceType = url.contains("pattern") ? ResourceType::Patterns : ResourceType::LayerStyles;
    item.lastModified = pkFileLastModified(location());
    return item;
}

KoResourceSP KisAslStorage::resource(const PkString &url)
{
    if (!m_aslSerializer->isInitialized()) {
        m_aslSerializer->readFromFile(location());
    }
    int indexOfUnderscore = pkLastIndexOf(url, "_");
    PkString realUuid = url;
    if (indexOfUnderscore >= 0) {
        realUuid = realUuid.left(indexOfUnderscore); // remove _pattern or _style added in iterator
    }
    // TODO: RESOURCES: Since we do get a resource type at the beginning of the path now
    //  maybe we could skip adding the _[resourcetype] at the end of the path as well?
    realUuid = pkBaseName(realUuid); // remove patterns/ at the beginning, if there are any

    if (url.contains("pattern") || url.contains(".pat")) {
        PkHash<PkString, KoPatternSP> patterns = m_aslSerializer->patterns();

        if (patterns.contains(realUuid)) {
            return patterns[realUuid];
        }
    }
    else {
        KisPSDLayerStyleSP resultingStyle;

        PkHash<PkString, KisPSDLayerStyleSP> styles = m_aslSerializer->stylesHash();
        if (styles.contains(realUuid)) {
            resultingStyle = styles[realUuid];
        } else {
            // can be {realUuid} or {realUuid}
            if (realUuid.startsWith("{")) {
                realUuid = realUuid.right(realUuid.size() - 1);
            }
            if (!realUuid.isEmpty() && realUuid.mid(realUuid.size() - 1) == "}") {
                realUuid = realUuid.left(realUuid.size() - 1);
            }

            if (styles.contains(realUuid)) {
                resultingStyle = styles[realUuid];
            }
        }

        if (resultingStyle) {
            KisPSDLayerStyleSP newStyle = resultingStyle->clone().dynamicCast<KisPSDLayerStyle>();

            // newStyle->resourcesInterface() is guaranteed to point to a local copy of the resouces
            // stored inside the serializer, so we need to side-load them and then reset the resources
            // interface to the global one
            KisAslLayerStyleSerializer::sideLoadLinkedResources(newStyle.data(), newStyle->resourcesInterface());
            newStyle->setResourcesInterface(KisGlobalResourcesInterface::instance());

            return newStyle;
        }
    }
    return 0;
}

bool KisAslStorage::loadVersionedResource(KoResourceSP /*resource*/)
{
    return false;
}

bool KisAslStorage::supportsVersioning() const
{
    return false;
}

PkSharedPointer<KisResourceStorage::ResourceIterator> KisAslStorage::resources(const PkString &resourceType)
{
    return PkSharedPointer<KisResourceStorage::ResourceIterator>(new AslIterator(m_aslSerializer, location(), resourceType));
}

PkSharedPointer<KisResourceStorage::TagIterator> KisAslStorage::tags(const PkString &resourceType)
{
    return PkSharedPointer<KisResourceStorage::TagIterator>(new AslTagIterator(location(), resourceType));
}

bool KisAslStorage::saveAsNewVersion(const PkString &/*resourceType*/, KoResourceSP /*resource*/)
{
    // not implemented yet
    warnKrita << "KisAslStorage::saveAsNewVersion is not implemented yet";
    return false;
}

bool KisAslStorage::addResource(const PkString &/*resourceType*/, KoResourceSP resource)
{
    if (!resource) {
        warnKrita << "Trying to add a null resource to KisAslStorage";
        return false;
    }
    KisPSDLayerStyleSP layerStyle = resource.dynamicCast<KisPSDLayerStyle>();
    if (!layerStyle) {
        warnKrita << "Trying to add a resource that is not a layer style to KisAslStorage";
        return false;
    }

    PkVector<KisPSDLayerStyleSP> styles = m_aslSerializer->styles();
    styles << layerStyle;
    m_aslSerializer->setStyles(styles);
    return m_aslSerializer->saveToFile(location());
}

bool KisAslStorage::isValid() const
{
    if (!m_aslSerializer->isInitialized()) {
        m_aslSerializer->readFromFile(location());
    }
    return m_aslSerializer->isValid();
}
