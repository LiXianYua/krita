/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2019 Agata Cacko <cacko.azh@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisAbrStorage.h"
#include "KisResourceStorage.h"

#include <QFileInfo>
#include <KisStaticInitializer.h>

KIS_DECLARE_STATIC_INITIALIZER {
    KisStoragePluginRegistry::instance()->addStoragePluginFactory(KisResourceStorage::StorageType::AdobeBrushLibrary, new KisStoragePluginFactory<KisAbrStorage>());
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
        abrTag->setUrl(QFileInfo(m_location).fileName());
        abrTag->setName(QFileInfo(m_location).fileName());
        abrTag->setComment(QFileInfo(m_location).fileName());
        abrTag->setFilename(QFileInfo(m_location).fileName());
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
    int indexOfUnderscore = url.lastIndexOf("_");
    PkString filenameUrl = url;
    // filenameUrl contains the name of the collection (filename without .abr, brush name without index)
    filenameUrl.remove(indexOfUnderscore, url.length() - indexOfUnderscore);
    item.folder = filenameUrl;
    item.resourceType = ResourceType::Brushes;
    item.lastModified = QFileInfo(m_brushCollection->filename()).lastModified();
    return item;
}


KoResourceSP KisAbrStorage::resource(const PkString &url)
{
    if (!m_brushCollection->isLoaded()) {
        m_brushCollection->load();
    }
    return m_brushCollection->brushByName(QFileInfo(url).fileName());
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
