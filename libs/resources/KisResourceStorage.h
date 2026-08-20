/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISRESOURCESTORAGE_H
#define KISRESOURCESTORAGE_H

#include <PkSharedPointer.h>
#include <PkScopedPointer.h>
#include <PkString.h>
#include <PkStringList.h>
#include <PkDateTime.h>
#include <PkMap.h>
#include <PkList.h>
#include <PkVector.h>
#include <PkVariant.h>
#include <PkImage.h>
#include <PkDebug.h>

#include <KoResource.h>
#include <KisTag.h>

#include <kritaresources_export.h>

class KisStoragePlugin;
class PkStream;

class KisStoragePluginFactoryBase
{
public:
    virtual ~KisStoragePluginFactoryBase(){}
    virtual KisStoragePlugin *create(const PkString &/*location*/) { return nullptr; }
};

template<typename T>
class KisStoragePluginFactory : public KisStoragePluginFactoryBase
{
public:
    KisStoragePlugin *create(const PkString &location) override {
        return new T(location);
    }
};

class KisResourceStorage;
typedef PkSharedPointer<KisResourceStorage> KisResourceStorageSP;


/**
 * The KisResourceStorage class is the base class for
 * places where resources can be stored. Examples are
 * folders, bundles or Adobe resource libraries like
 * ABR files.
 */
class KRITARESOURCES_EXPORT KisResourceStorage
{
public:

    /// A resource item is simply an entry in the storage,
    struct ResourceItem {

        virtual ~ResourceItem() {}
        PkString url;
        PkString folder;
        PkString resourceType;
        PkDateTime lastModified;
    };

    class KRITARESOURCES_EXPORT TagIterator
    {
    public:
        virtual ~TagIterator() {}
        virtual bool hasNext() const = 0;
        /// The iterator is only valid if next() has been called at least once.
        virtual void next() = 0;

        /// A tag object on which we can set properties and which we can save
        virtual KisTagSP tag() const = 0;
    };

    class KRITARESOURCES_EXPORT ResourceIterator
    {
    public:

        virtual ~ResourceIterator() {}

        virtual bool hasNext() const = 0;
        /// The iterator is only valid if next() has been called at least once.
        virtual void next() = 0;

        virtual PkString url() const = 0;
        virtual PkString type() const = 0;
        virtual PkDateTime lastModified() const = 0;
        virtual int guessedVersion() const { return 0; }
        virtual PkSharedPointer<KisResourceStorage::ResourceIterator> versions() const;

        KoResourceSP resource() const;

    protected:
        /// This only loads the resource when called
        virtual KoResourceSP resourceImpl() const = 0;

    private:
        mutable KoResourceSP m_cachedResource;
        mutable PkString m_cachedResourceUrl;
    };

    enum class StorageType : int {
        Unknown = 1,
        Folder = 2,
        Bundle = 3,
        AdobeBrushLibrary = 4,
        AdobeStyleLibrary = 5,
        Memory = 6,
        FontStorage = 7
    };

    static PkString storageTypeToString(StorageType storageType) {
        switch (storageType) {
        case StorageType::Unknown:
            return PkString("Unknown");
        case StorageType::Folder:
            return PkString("Folder");
        case StorageType::Bundle:
            return PkString("Bundle");
        case StorageType::AdobeBrushLibrary:
            return PkString("Adobe Brush Library");
        case StorageType::AdobeStyleLibrary:
            return PkString("Adobe Style Library");
        case StorageType::FontStorage:
            return PkString("Font Storage");
        case StorageType::Memory:
            return PkString("Memory");
        default:
            return PkString("Invalid");
        }
    }


    static PkString storageTypeToUntranslatedString(StorageType storageType) {
        switch (storageType) {
        case StorageType::Unknown:
            return ("Unknown");
        case StorageType::Folder:
            return ("Folder");
        case StorageType::Bundle:
            return ("Bundle");
        case StorageType::AdobeBrushLibrary:
            return ("Adobe Brush Library");
        case StorageType::AdobeStyleLibrary:
            return ("Adobe Style Library");
        case StorageType::FontStorage:
            return ("Font Storage");
        case StorageType::Memory:
            return ("Memory");
        default:
            return ("Invalid");
        }
    }


    KisResourceStorage(const PkString &location, KisResourceStorage::StorageType storageType);
    KisResourceStorage(const PkString &location);
    ~KisResourceStorage();
    KisResourceStorage(const KisResourceStorage &rhs);
    KisResourceStorage &operator=(const KisResourceStorage &rhs);
    KisResourceStorageSP clone() const;

    /// The filename of the storage if it's a bundle or Adobe Library. This can
    /// also be empty (for the folder storage) or "memory" for the storage for
    /// temporary resources, a UUID for storages associated with documents.
    PkString name() const;

    /// The absolute location of the storage
    PkString location() const;

    /// true if the storage exists and can be used
    bool valid() const;

    /// The type of the storage
    StorageType type() const;

    /// The icond for the storage
    PkImage thumbnail() const;

    /// The time and date when the storage was last modified, or created
    /// for memory storages.
    PkDateTime timestamp() const;

    /// The time and date when the resource was last modified
    /// For filestorage
    PkDateTime timeStampForResource(const PkString &resourceType, const PkString &filename) const;

    /// And entry in the storage; this is not the loaded resource
    ResourceItem resourceItem(const PkString &url);

    /// The loaded resource for an entry in the storage
    KoResourceSP resource(const PkString &url);

    /// The MD5 checksum of the resource in the storage
    PkString resourceMd5(const PkString &url);

    /// If the resource is present on the filesystem as a distinct fine,
    /// returns the full file path of it, otherwise returns an empty string.
    ///
    /// Never manipulate the file in any way directly! It will destroy the
    /// resources database. Use this file path only for informational purposes.
    PkString resourceFilePath(const PkString &url);

    /// An iterator over all the resources in the storage
    PkSharedPointer<ResourceIterator> resources(const PkString &resourceType) const;

    /// An iterator over all the tags in the resource
    PkSharedPointer<TagIterator> tags(const PkString &resourceType) const;

    /// Adds a tag to the storage, however, it does not store the links between
    /// tags and resources.
    bool addTag(const PkString &resourceType, KisTagSP tag);

    /// Creates a new version of the given resource.
    bool saveAsNewVersion(KoResourceSP resource);

    /// Adds the given resource to the storage. If there is already a resource
    /// with the given filename of the given type, this should return false and
    /// saveAsNewVersion should be used.
    bool addResource(KoResourceSP resource);

    /**
     * Copies the given file into this storage. Implementations should not overwrite
     * an existing resource with the same filename, but return false.
     *
     * @param url is the URL of the resource inside the storage, which is usually
     *            resource_type/resource_filename.ext
     */
    bool importResource(const PkString &url, PkStream *device);

    /**
     * Copies the given resource from the storage into \p device
     *
     * @param url is the URL of the resource inside the storage, which is usually
     *            resource_type/resource_filename.ext
     */
    bool exportResource(const PkString &url, PkStream *device);

    /// Returns true if the storage supports versioning of the resources.
    /// It enables loadVersionedResource() call.
    bool supportsVersioning() const;

    /// Reloads the given resource from the persistent storage
    bool loadVersionedResource(KoResourceSP resource);

    static const PkString s_xmlns_meta;
    static const PkString s_xmlns_dc;

    static const PkString s_meta_generator;
    static const PkString s_meta_author;
    static const PkString s_meta_title;
    static const PkString s_meta_description;
    static const PkString s_meta_initial_creator;
    static const PkString s_meta_creator;
    static const PkString s_meta_creation_date;
    static const PkString s_meta_dc_date;
    static const PkString s_meta_user_defined;
    static const PkString s_meta_name;
    static const PkString s_meta_value;
    static const PkString s_meta_version;
    static const PkString s_meta_license;
    static const PkString s_meta_email;
    static const PkString s_meta_website;

    void setMetaData(const PkString &key, const PkVariant &value);
    PkStringList metaDataKeys() const;
    PkVariant metaData(const PkString &key) const;

private:

    friend class KisStorageModel;
    friend class KisResourceLocator;
    friend class KisResourceCacheDb;
    friend class TestResourceLocator;
    friend class TestStorageWrapper;

    KisStoragePlugin* testingGetStoragePlugin();

    void setStorageId(int storageId);
    int storageId();

    class Private;
    PkScopedPointer<Private> d;
};



inline PkDebug operator<<(PkDebug dbg, const KisResourceStorageSP storage)
{
    if (storage.isNull()) {
        dbg.nospace() << "[RESOURCESTORAGE] NULL";
    }
    else {
        dbg.nospace() << "[RESOURCESTORAGE] Name: " << storage->name()
                      << " Location: " << storage->location()
                      << " Valid: " << storage->valid()
                      << " Storage: " << KisResourceStorage::storageTypeToString(storage->type())
                      << " Timestamp: " << storage->timestamp()
                      << " Pointer: " << storage.data();
    }
    return dbg.space();
}

class KRITARESOURCES_EXPORT KisStoragePluginRegistry {
public:
    KisStoragePluginRegistry();
    virtual ~KisStoragePluginRegistry();

    void addStoragePluginFactory(KisResourceStorage::StorageType storageType, KisStoragePluginFactoryBase *factory);
    PkList<KisResourceStorage::StorageType> storageTypes() const;
    static KisStoragePluginRegistry *instance();
private:
    friend class KisResourceStorage;
    PkMap<KisResourceStorage::StorageType, KisStoragePluginFactoryBase*> m_storageFactoryMap;

};

struct VersionedResourceEntry
{
    PkString resourceType;
    PkString filename;
    PkList<PkString> tagList;
    PkDateTime lastModified;
    int guessedVersion = -1;
    PkString guessedKey;

    struct KeyVersionLess {
        bool operator()(const VersionedResourceEntry &lhs, const VersionedResourceEntry &rhs) const {
            return lhs.guessedKey < rhs.guessedKey ||
                (lhs.guessedKey == rhs.guessedKey && lhs.guessedVersion < rhs.guessedVersion);
        }
    };

    struct KeyLess {
        bool operator()(const VersionedResourceEntry &lhs, const VersionedResourceEntry &rhs) const {
            return lhs.guessedKey < rhs.guessedKey;
        }
    };
};

class KRITARESOURCES_EXPORT KisStorageVersioningHelper {
public:

    static bool addVersionedResource(const PkString &saveLocation, KoResourceSP resource, int minVersion);
    static PkString chooseUniqueName(KoResourceSP resource,
                                    int minVersion,
                                    std::function<bool(PkString)> checkExists);

    static void detectFileVersions(PkVector<VersionedResourceEntry> &allFiles);


};

class KisVersionedStorageIterator : public KisResourceStorage::ResourceIterator
{
public:
    KisVersionedStorageIterator(const PkVector<VersionedResourceEntry> &entries,
                                KisStoragePlugin *_q);

    bool hasNext() const override;
    void next() override;
    PkString url() const override;
    PkString type() const override;
    PkDateTime lastModified() const override;
    KoResourceSP resourceImpl() const override;

    int guessedVersion() const override;

    PkSharedPointer<KisResourceStorage::ResourceIterator> versions() const override;

protected:
    KisVersionedStorageIterator(const PkVector<VersionedResourceEntry> &entries,
                                PkVector<VersionedResourceEntry>::const_iterator begin,
                                PkVector<VersionedResourceEntry>::const_iterator end,
                                KisStoragePlugin *_q);
protected:
    KisStoragePlugin *q = 0;
    const PkVector<VersionedResourceEntry> m_entries;
    PkVector<VersionedResourceEntry>::const_iterator m_it;
    PkVector<VersionedResourceEntry>::const_iterator m_chunkStart;
    PkVector<VersionedResourceEntry>::const_iterator m_begin;
    PkVector<VersionedResourceEntry>::const_iterator m_end;
    bool m_isStarted = false;
};


#endif // KISRESOURCESTORAGE_H
