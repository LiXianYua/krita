/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISRESOURCECACHEDB_H
#define KISRESOURCECACHEDB_H

#include <kritaresources_export.h>

#include <KisResourceStorage.h>
#include <PkAuxTypes.h>
#include <PkDateTime.h>
#include <PkMap.h>
#include <PkStringList.h>
#include <PkVariant.h>
#include <PkVector.h>

#include <vector>

/**
 * @brief The KisResourceCacheDb class encapsulates the database that
 * caches information about the resources available to the user.
 *
 * KisApplication creates and initializes the database. All other methods
 * are static and can be used from anywhere.
 */
class KRITARESOURCES_EXPORT KisResourceCacheDb
{
public:

    static const PkString resourceCacheDbFilename; ///< filename of the database
    static const PkString databaseVersion; ///< current schema version
    static PkStringList storageTypes; ///< kinds of places where resources can be stored
    static PkStringList disabledBundles; ///< the list of compatibility bundles that need to inactive by default

    /**
     * @brief isValid
     * @return true if the database has been correctly created, false if the database cannot be used
     */
    static bool isValid();

    /**
     * @brief lastError returns the last SQL error.
     */
    static PkString lastError();

    /**
     * @brief initializes the database and updates the scheme if necessary. Does not actually
     * fill the database with pointers to resources.
     *
     * @param location the location of the database
     * @return true if the database has been initialized correctly
     */
    static bool initialize(const PkString &location);

    /// Delete all storages that are Unknown or Memory and all resources that are marked temporary or belong to Unknown or Memory storages
    static bool deleteTemporaryResources();

    /// perform optimize and vacuum when necessary
    static void performHouseKeepingOnExit();

    /// set the foreign_keys feature state of the database
    /// (the function **may** throw SQL exceptions,
    /// call in a try-block only!)
    static void setForeignKeysStateImpl(bool isEnabled);

    /// get the foreign_keys feature state of the database
    /// (the function **may** throw SQL exceptions,
    /// call in a try-block only!)
    static bool getForeignKeysStateImpl();

    /// Called in the end of the database creation step to enable
    /// or disable the foreign_keys state depending on the release
    /// status of Krita. Currently, only developer's builds of Krita
    /// have foreign_keys constraint enabled.
    static void synchronizeForeignKeysState();

    enum class MetaDataDecodeStatus
    {
        InvalidBase64,
        UnsupportedUserType,
        UnsupportedType,
        ReadPastEnd,
        ReadCorruptData,
        TrailingData,
        PayloadLimitExceeded,
        InvalidUtf8,
        UnsupportedStorageClass
    };

    enum class MetaDataStorageClass
    {
        Null,
        Integer,
        Real,
        Text,
        Blob,
        Unknown
    };

    struct MetaDataDecodeIssue
    {
        MetaDataDecodeStatus status = MetaDataDecodeStatus::ReadCorruptData;
        PkString rawPayload;
        bool rawPayloadAvailable = true;
    };

    /**
     * One physical metadata row. The row id and SQLite storage classes are
     * retained even when the key/value cannot be decoded. Raw bytes are exact
     * only when the corresponding availability flag is true.
     */
    struct MetaDataReadRow
    {
        long long rowId = -1;
        MetaDataStorageClass keyStorageClass = MetaDataStorageClass::Unknown;
        MetaDataStorageClass valueStorageClass = MetaDataStorageClass::Unknown;
        PkByteArray rawKey;
        bool rawKeyAvailable = false;
        PkByteArray rawPayload;
        bool rawPayloadAvailable = false;
        PkString key;
        bool keyAvailable = false;
        PkVariant value;
        bool decoded = false;
        MetaDataDecodeStatus status = MetaDataDecodeStatus::ReadCorruptData;
    };

    struct MetaDataReadResult
    {
        // Authoritative row-preserving view. Unlike the compatibility maps
        // below, this cannot conflate different SQLite storage classes or
        // lossy UTF-8 normalizations under one PkString key. A failed page or
        // incremental column read is all-or-error: querySucceeded is false
        // and every projection below is empty. resourceLimitExceeded instead
        // describes a successful, explicitly bounded read.
        std::vector<MetaDataReadRow> rows;
        PkMap<PkString, PkVariant> values;
        PkMap<PkString, MetaDataDecodeIssue> undecodable;
        bool querySucceeded = false;
        bool resourceLimitExceeded = false;
    };

    /**
     * Reads metadata without conflating an undecodable persisted value with a
     * missing key. The row-preserving view retains SQLite storage classes and
     * bounded exact bytes; values/undecodable are compatibility projections
     * for valid UTF-8 TEXT keys only.
     */
    static MetaDataReadResult metaDataReadResultForId(int id,
                                                      const PkString &tableName);

private:

    friend class KisResourceLocator;
    friend class TestResourceLocator;
    friend class TestResourceCacheDb;
    friend class KisAllTagsModel;
    friend class KisResourceLoaderRegistry;
    friend class KisResourceUserOperations;
    friend class KisDocument;
    friend class KisAllResourcesModel;

    explicit KisResourceCacheDb(); // Deleted
    ~KisResourceCacheDb(); // Deleted
    KisResourceCacheDb operator=(const KisResourceCacheDb&); // Deleted
    /**
     * @brief registerResourceType registers this resource type in the database
     * @param resourceType the string that represents the type
     * @return true if the type was registered or had already been registered
     */
    static bool registerResourceType(const PkString &resourceType);

    /**
     * Returns a list of tags related to the storage
     *
     * The first item of the pair represents the tags that are linked to
     * this very storage uniquely. The second item of the pair lists the
     * tags that are shared with other storages.
     */
    static std::pair<PkVector<int>,PkVector<int>> tagsForStorage(const PkString &resourceType, const PkString &storageLocation);
    /**
     * Returns a list of resources owned by the storage
     */
    static PkVector<int> resourcesForStorage(const PkString &resourceType, const PkString &storageLocation);
    static int resourceIdForResource(const PkString &resourceFileName, const PkString &resourceType, const PkString &storageLocation);
    static bool resourceNeedsUpdating(int resourceId, PkDateTime timestamp);

    /**
     * @brief addResourceVersion adds a new version of the resource to the database.
     * The resource itself already should be updated with the updated filename and version.
     * @param resourceId unique identifier for the resource
     * @param timestamp
     * @param storage
     * @param resource
     * @return true if the database was successfully updated
     */
    static bool addResourceVersion(int resourceId, PkDateTime timestamp, KisResourceStorageSP storage, KoResourceSP resource);

    static bool addResourceVersionImpl(int resourceId, PkDateTime timestamp, KisResourceStorageSP storage, KoResourceSP resource);
    static bool removeResourceVersionImpl(int resourceId, int version, KisResourceStorageSP storage);

    static bool updateResourceTableForResourceIfNeeded(int resourceId, const PkString &resourceType, KisResourceStorageSP storage);
    static bool makeResourceTheCurrentVersion(int resourceId, KoResourceSP resource);
    static bool removeResourceCompletely(int resourceId);

    /// The function will find the resource only if it is the latest version
    static bool getResourceIdFromFilename(PkString filename, PkString resourceType, PkString storageLocation, int &outResourceId);
    /// Note that here you can put even the original filename - any filename from the versioned_resources - and it will still find it
    static bool getResourceIdFromVersionedFilename(PkString filename, PkString resourceType, PkString storageLocation, int& outResourceId);
    static bool getAllVersionsLocations(int resourceId, PkStringList &outVersionsLocationsList);


    static bool addResource(KisResourceStorageSP storage, PkDateTime timestamp, KoResourceSP resource, const PkString &resourceType);
    static bool addResources(KisResourceStorageSP storage, PkString resourceType);

    /// Make this resource active or inactive; this does not remove the resource from disk or from the database
    static bool setResourceActive(int resourceId, bool active = false);

    static bool tagResource(const PkString &resourceFileName, KisTagSP tag, const PkString &resourceType);
    static bool hasTag(const PkString &url, const PkString &resourceType);
    static bool linkTagToStorage(const PkString &url, const PkString &resourceType, const PkString &storageLocation);
    static bool addTag(const PkString &resourceType, const PkString storageLocation, KisTagSP tag);
    static bool addTags(KisResourceStorageSP storage, PkString resourceType);

    /**
     * @brief registerStorageType registers this storage type in the database
     * @param storageType the enum value that represents the type
     * @return true if the type was registered or had already been registered
     */
    static bool registerStorageType(const KisResourceStorage::StorageType storageType);
    static bool addStorage(KisResourceStorageSP storage, bool preinstalled);
    static bool addStorageTags(KisResourceStorageSP storage);

    /// Actually delete the storage and all its resources from the database (i.e., nothing is set to inactive, it's deleted)
    static bool deleteStorage(KisResourceStorageSP storage);
    /// Actually delete the storage and all its resources from the database (i.e., nothing is set to inactive, it's deleted)
    ///  location - relative
    static bool deleteStorage(PkString location);
    static bool synchronizeStorage(KisResourceStorageSP storage);

    /**
     * @brief metaDataForId
     * @param id
     * @param tableName
     * @return
     */
    static PkMap<PkString, PkVariant> metaDataForId(int id, const PkString &tableName);

    /**
     * @brief setMetaDataForId removes all metadata for the given id and table name,
     *  and inserts the metadata in the metadata table.
     * @param id
     * @param tableName
     * @return true if successful, false if not
     */
    static bool updateMetaDataForId(const PkMap<PkString, PkVariant> map, int id, const PkString &tableName);
    static bool addMetaDataForId(const PkMap<PkString, PkVariant> map, int id, const PkString &tableName);

    /**
     * @brief removeOrphanedMetaData
     * Previous versions of Krita never removed metadata, so this function doublechecks and
     * removes any orphaned metadata for either storages or resources from the database.
     * @return true if successful, false if not
     */
    static bool removeOrphanedMetaData();

    static bool s_valid;
    static PkString s_lastError;
};

#endif // KISRESOURCECACHEDB_H
