/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisAbrStorage.h"
#include "KisResourceStorage.h"

#include <chrono>
#include <filesystem>
#include <system_error>
#include <KisStaticInitializer.h>

KIS_DECLARE_STATIC_INITIALIZER {
    KisStoragePluginRegistry::instance()->addStoragePluginFactory(KisResourceStorage::StorageType::AdobeBrushLibrary, new KisStoragePluginFactory<KisAbrStorage>());
}

// QFileInfo 在 migrate 后无 Pk 等价（QFileInfo 无 PkString 构造），
// 按 S-02-b PkResourceStorageDesktop::lastModifiedMs 的模式用 std::filesystem
// 复刻「PkString 路径 → 文件名 / 最后修改时间」。
static PkString pathFileName(const PkString &path)
{
    const std::string name = std::filesystem::u8path(path.PkToUtf8()).filename().string();
    return PkString::PkFromUtf8(name.c_str(), static_cast<int>(name.size()));
}

static PkDateTime pathLastModified(const PkString &path)
{
    std::error_code ec;
    const std::filesystem::file_time_type writeTime = std::filesystem::last_write_time(path.PkToUtf8(), ec);
    if (ec) {
        return PkDateTime();
    }
    const auto systemTime = std::chrono::time_point_cast<std::chrono::milliseconds>(
        writeTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    return PkDateTime::fromMSecsSinceEpoch(systemTime.time_since_epoch().count());
}

class AbrTagIterator : public KisResourceStorage::TagIterator
{
public:
    AbrTagIterator(KisAbrBrushCollectionSP brushCollection, const PkString &location, const PkString &resourceType)
        : m_brushCollection(brushCollection)
        , m_location(location)
        , m_resourceType(resourceType)
    {}

    bool hasNext() const override {
        if (m_resourceType != ResourceType::Brushes) return false;
        return !m_taggingDone;
    }

    void next() override { m_taggingDone = true; }

    KisTagSP tag() const override
    {
        KisTagSP abrTag(new KisTag());
        abrTag->setUrl(pathFileName(m_location));
        abrTag->setName(pathFileName(m_location));
        abrTag->setComment(pathFileName(m_location));
        abrTag->setFilename(pathFileName(m_location));
        abrTag->setResourceType(m_resourceType);
        abrTag->setValid(true);
        PkStringList brushes;
        Q_FOREACH(const KisAbrBrushSP brush, m_brushCollection->brushes()) {
            brushes << brush->filename();
        }
        abrTag->setDefaultResources(brushes);

        return abrTag;
    }

private:

    bool m_taggingDone {false};
    KisAbrBrushCollectionSP m_brushCollection;
    PkString m_location;
    PkString m_resourceType;
};

class AbrIterator : public KisResourceStorage::ResourceIterator
{
public:
    KisAbrBrushCollectionSP m_brushCollection;
    PkSharedPointer<PkMap<PkString, KisAbrBrushSP>> m_brushesMap;
    PkMap<PkString, KisAbrBrushSP>::const_iterator m_brushCollectionIterator;
    KisAbrBrushSP m_currentResource;
    bool isLoaded;
    PkString m_currentUrl;
    PkString m_resourceType;


    AbrIterator(KisAbrBrushCollectionSP brushCollection, const PkString& resourceType)
        : m_brushCollection(brushCollection)
        , isLoaded(false)
        , m_resourceType(resourceType)
    {
    }

    bool hasNext() const override
    {
        if (m_resourceType != ResourceType::Brushes) {
            return false;
        }

        if (!isLoaded) {
            bool success = m_brushCollection->load();
            Q_UNUSED(success); // brush collection will be empty
            const_cast<AbrIterator*>(this)->m_brushesMap = m_brushCollection->brushesMap();
            const_cast<AbrIterator*>(this)->m_brushCollectionIterator = m_brushesMap->constBegin();
            const_cast<AbrIterator*>(this)->isLoaded = true;
        }

        if (m_brushCollectionIterator == m_brushesMap->constEnd()) {
            return false;
        }

        bool hasNext = (m_brushCollectionIterator != m_brushesMap->constEnd());
        return hasNext;
    }

    void next() override
    {
        if (m_resourceType != ResourceType::Brushes) {
            return;
        }
        KIS_SAFE_ASSERT_RECOVER_RETURN(m_brushCollectionIterator != m_brushesMap->constEnd());
        m_currentResource = m_brushCollectionIterator.value();
        m_currentUrl = m_brushCollectionIterator.key();
        m_brushCollectionIterator++;
    }

    PkString url() const override { return m_currentUrl; }
    PkString type() const override { return ResourceType::Brushes; }
    PkDateTime lastModified() const override { return m_brushCollection->lastModified(); }

    KoResourceSP resourceImpl() const override
    {
        return m_currentResource;
    }
};

KisAbrStorage::KisAbrStorage(const PkString &location)
    : KisStoragePlugin(location)
    , m_brushCollection(new KisAbrBrushCollection(location))
{
}

KisAbrStorage::~KisAbrStorage()
{

}

KisResourceStorage::ResourceItem KisAbrStorage::resourceItem(const PkString &url)
{
    KisResourceStorage::ResourceItem item;
    item.url = url;
    // last "_" with index is the suffix added by abr_collection
    // PkString 无 lastIndexOf/remove/length：手动反向扫描定位最后一个 '_'，
    // 用 left() 取集合名（文件名去掉 .abr、笔刷名去掉索引后缀）。
    int indexOfUnderscore = -1;
    for (int i = url.size() - 1; i >= 0; --i) {
        if (url.at(i) == u'_') {
            indexOfUnderscore = i;
            break;
        }
    }
    PkString filenameUrl = url;
    if (indexOfUnderscore >= 0) {
        filenameUrl = url.left(indexOfUnderscore);
    }
    item.folder = filenameUrl;
    item.resourceType = ResourceType::Brushes;
    item.lastModified = pathLastModified(m_brushCollection->filename());
    return item;
}


KoResourceSP KisAbrStorage::resource(const PkString &url)
{
    if (!m_brushCollection->isLoaded()) {
        m_brushCollection->load();
    }
    return m_brushCollection->brushByName(pathFileName(url));
}

bool KisAbrStorage::loadVersionedResource(KoResourceSP /*resource*/)
{
    return false;
}

bool KisAbrStorage::supportsVersioning() const
{
    return false;
}

PkSharedPointer<KisResourceStorage::ResourceIterator> KisAbrStorage::resources(const PkString &resourceType)
{
    return PkSharedPointer<KisResourceStorage::ResourceIterator>(new AbrIterator(m_brushCollection, resourceType));
}

PkSharedPointer<KisResourceStorage::TagIterator> KisAbrStorage::tags(const PkString &resourceType)
{
    return PkSharedPointer<KisResourceStorage::TagIterator>(new AbrTagIterator(m_brushCollection, location(), resourceType));
}

PkImage KisAbrStorage::thumbnail() const
{
    return m_brushCollection->image();
}
