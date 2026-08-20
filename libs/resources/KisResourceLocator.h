/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISRESOURCELOCATOR_H
#define KISRESOURCELOCATOR_H

#include <PkObject.h>
#include <PkList.h>
#include <PkMap.h>
#include <PkScopedPointer.h>
#include <PkStringList.h>
#include <PkString.h>
#include <PkVariant.h>
#include <PkVector.h>

#include "kritaresources_export.h"

#include <KisResourceStorage.h>


/**
 * The KisResourceLocator class locates all resource storages (folders,
 * bundles, various adobe resource libraries) in the resource location.
 *
 * The resource location is always a writable folder.
 *
 * There is one process-wide resource locator.
 *
 * The resource location is configurable, but there is only one location
 * where Krita will look for resources.
 */
class KRITARESOURCES_EXPORT KisResourceLocator : public PkObject
{
public:

    // The configuration key that holds the resource location
    // for this installation of Krita. The location is
    // The platform application-data location is the default, but that
    // can be changed.
    static const PkString resourceLocationKey;

    /**
     * Return the process-wide locator, creating it on first use.
     *
     * Once shutdown() begins this returns nullptr forever. Callers must stop
     * all locator use and disconnect every raw pointer before shutdown().
     */
    static KisResourceLocator *instance();

    /**
     * Return the live locator without creating it, or nullptr when it has not
     * been created or shutdown has begun.
     */
    static KisResourceLocator *existingInstance();

    /**
     * Destroy the process-wide locator before resource-storage plugins are
     * unloaded. The call is idempotent and terminal: instance() cannot create
     * a new generation after shutdown begins.
     *
     * Application teardown must first disconnect/stop all raw-pointer users,
     * then call KisResourceThumbnailCache::shutdown(), this function, and
     * finally KisResourceLoaderRegistry::shutdown(), in that order.
     */
    static void shutdown();

    ~KisResourceLocator();

    enum class LocatorError {
        Ok,
        LocationReadOnly,
        CannotCreateLocation,
        CannotInitializeDb,
        CannotSynchronizeDb
    };

    /**
     * @brief initialize Setup the resource locator for use.
     *
     * @param installationResourcesLocation the place where the resources
     * that come packaged with Krita reside.
     */
    LocatorError initialize(const PkString &installationResourcesLocation);

    /**
     * @brief errorMessages
     * @return
     */
    PkStringList errorMessages() const;

    /**
     * @brief resourceLocationBase is the place where all resource storages (folder,
     * bundles etc. are located. This is a writable place.
     * @return the base location for all storages.
     */
    PkString resourceLocationBase() const;

    /**
     * @brief purge purges the local resource cache
     */
    void purge(const PkString &storageLocation, const PkVector<int> &removedTagIds);

    /**
     * @brief addStorage Adds a new resource storage to the database. The storage is
     * will be marked as not pre-installed. If there is already a storage with the
     * given location, it will first be removed.
     * @param storageLocation a unique name for the given storage
     * @param storage a storage object
     * @return true if the storage has been added successfully
     */
    bool addStorage(const PkString &storageLocation, KisResourceStorageSP storage);

    /**
     * @brief removeStorage removes the temporary storage from the database
     * @param storageLocation the unique name of the storage
     * @return true is successful.
     */
    bool removeStorage(const PkString &storageLocation);

    /**
     * @brief hasStorage can be used to check whether the given storage already exists
     * @param storageLocation the name of the storage
     * @return true if the storage is known
     */
    bool hasStorage(const PkString &storageLocation);


    /**
     * @brief saveTags saves all tags to .tag files in the resource folder
     */
    static void saveTags();

    /**
     * Remove the given tag from the cache
     */
    void purgeTag(const PkString tagUrl, const PkString resourceType);

    /**
     * Returns the full file path of the resource if it has any
     * separate physical representation on the disk
     */
    PkString filePathForResource(KoResourceSP resource);

    /// This updates the "fontregistry" storage. Called when the font directories change;
    void updateFontStorage();

    // Pk signal declarations. Definitions dispatch through
    // PkObject::activateSignal in KisResourceLocator.cpp.

    void progressMessage(const PkString&);

    /// Emitted whenever a storage is added
    void storageAdded(const PkString &location);

    /// Emitted whenever a storage is removed
    void storageRemoved(const PkString &location);

    /// Emitted when the locator needs to add an embedded resource
    void beginExternalResourceImport(const PkString &resourceType, int numResources);

    /// Emitted when the locator finished importing the embedded resource
    void endExternalResourceImport(const PkString &resourceType);

    /// Emitted when the locator needs to add an embedded resource
    void beginExternalResourceRemove(const PkString &resourceType, const PkVector<int> resourceIds);

    /// Emitted when the locator finished importing the embedded resource
    void endExternalResourceRemove(const PkString &resourceType);

    /// Emitted when a resource changes its active state
    void resourceActiveStateChanged(const PkString &resourceType, int resourceId);

    /// Emitted when a storage is resynchronized using KisresourceCacheDb::synchronizeStorage()
    ///
    /// if \p isBulkResynchronization then this resynchronization happened as a part
    /// of bulk resynchronization at the start of Krita. At the end of this bulk
    /// action storagesBulkSynchronizationFinished() will be emitted as well.
    void storageResynchronized(const PkString &storage, bool isBulkResynchronization);

    /// Emitted when bulk-synchronization of all the storages has been finished
    ///
    /// \see storageResynchronized
    void storagesBulkSynchronizationFinished();

private:

    friend class KisAllTagsModel;
    friend class KisTagResourceModel;
    friend class KisAllResourcesModel;
    friend class KisAllTagResourceModel;
    friend class KisStorageModel;
    friend class TestResourceLocator;
    friend class TestResourceModel;
    friend class Resource;
    friend class KisResourceCacheDb;
    friend class KisStorageFilterProxyModel;
    friend class KisResourceQueryMapper;
    friend class KisResourceUserOperations;
    friend class KisBrushTypeMetaDataFixup;
    friend class KisResourceThumbnailCache;

    /// @return true if the resource is present in the cache, false if it hasn't been loaded
    bool resourceCached(PkString storageLocation, const PkString &resourceType, const PkString &filename) const;

    /**
     * @brief resource finds a physical resource in one of the storages
     * @param storageLocation the storage containing the resource. If empty,
     * this is the folder storage.
     *
     * Note that the resource does not have the version or id field set, so this cannot be used directly,
     * but only through KisResourceModel.
     *
     * @param resourceType the type of the resource
     * @param filename the filename of the resource including extension, but without
     * any paths
     * @return A resource if found, or 0
     */
    KoResourceSP resource(PkString storageLocation, const PkString &resourceType, const PkString &filename);

    /**
     * @brief resourceForId returns the resource with the given id, or 0 if no such resource exists.
     * The resource object will have its id set but not its version.
     * @param resourceId the id
     */
    KoResourceSP resourceForId(int resourceId);

    /**
     * @brief setResourceActive
     * @param resourceId
     * @param active shows if the resource should be set as active or not
     * @return
     */
    bool setResourceActive(int resourceId, bool active);

    /**
     * @brief importResourceFromFile
     * @param resourceType
     * @param fileName
     * @param storageLocation: optional, the storage where the resource will be stored. Empty means in the default Folder storage.
     * @return the imported resource, which has been added to the database and the cache
     */
    KoResourceSP importResourceFromFile(const PkString &resourceType, const PkString &fileName, const bool allowOverwrite, const PkString &storageLocation = PkString());

    /**
     * @brief importResource
     * @param resourceType
     * @param fileName: filename that should be assigned to the resource
     * @param device: PkStream where the resource should be loaded from
     * @param storageLocation: optional, the storage where the resource will be stored. Empty means in the default Folder storage.
     * @return the imported resource, which has been added to the database and the cache
     */
    KoResourceSP importResource(const PkString &resourceType, const PkString &fileName, PkStream *device, const bool allowOverwrite, const PkString &storageLocation = PkString());

    /**
     * @brief importResourceDeduplicateFileName imports the resource fith file name deduplication
     *
     * When loading embedded resources from another resources, we should make sure they
     * do not overwrite anything and land in the database unconditionally. That is why we might
     * need to rename them on loading. The parent resource will (hopefully) still be able to
     * address them using md5sum.
     *
     * @param resourceType
     * @param proposedFileName: filename that should be assigned to the resource (will possibly be changed)
     * @param device: PkStream where the resource should be loaded from
     * @param storageLocation: optional, the storage where the resource will be stored. Empty means in the default Folder storage.
     * @return the imported resource, which has been added to the database and the cache
     */
    KoResourceSP importResourceDeduplicateFileName(const PkString &resourceType, const PkString &proposedFileName, PkStream *device, const PkString &storageLocation = PkString());

    /**
     * @brief addResourceDeduplicateFileName imports the resource fith file name deduplication
     *
     * When loading embedded resources from another resources, we should make sure they
     * do not overwrite anything and land in the database unconditionally. That is why we might
     * need to rename them on loading. The parent resource will (hopefully) still be able to
     * address them using md5sum.
     *
     * @param resourceType
     * @param device: PkStream where the resource should be loaded from
     * @param storageLocation: optional, the storage where the resource will be stored. Empty means in the default Folder storage.
     * @return the imported resource, which has been added to the database and the cache
     */
    bool addResourceDeduplicateFileName(const PkString &resourceType, const KoResourceSP resource, const PkString &storageLocation);

    /**
     * @brief return whether importing will overwrite some existing resource
     * @param resourceType
     * @param fileName: filename that should be assigned to the resource
     * @param storageLocation: optional, the storage where the resource will be stored. Empty means in the default Folder storage.
     */
    bool importWillOverwriteResource(const PkString &resourceType, const PkString &fileName, const PkString &storageLocation = PkString()) const;

    /**
     * @brief exportResource
     * @param resource resource to be exported
     * @param device: PkStream where the resource should be loaded to
     * @return true if the resource has been exported successfully
     */
    bool exportResource(KoResourceSP resource, PkStream *device);

    /**
     * @brief addResource adds the given resource to the database and potentially a storage
     * @param resourceType the type of the resource
     * @param resource the actual resource object
     * @param storageLocation the storage where the resource will be saved. By default this is the default folder storage.
     * @return true if successful
     */
    bool addResource(const PkString &resourceType, const KoResourceSP resource, const PkString &storageLocation = PkString());

    /**
     * @brief updateResource
     * @param resourceType
     * @param resource
     * @return
     */
    bool updateResource(const PkString &resourceType, const KoResourceSP resource);

    /**
     * @brief Reloads the resource from its persistent storage
     * @param resourceType the type of the resource
     * @param resource the actual resource object
     * @return true if reloading was successful. When returned false,
     *         \p resource is kept unchanged
     */
    bool reloadResource(const PkString &resourceType, const KoResourceSP resource);

    /**
     * @brief metaDataForResource
     * @param id
     * @return
     */
    PkMap<PkString, PkVariant> metaDataForResource(int id) const;

    /**
     * @brief setMetaDataForResource
     * @param id
     * @param map
     * @return
     */
    bool setMetaDataForResource(int id, PkMap<PkString, PkVariant> map) const;

    /**
     * @brief metaDataForStorage
     * @param storage
     * @return
     */
    PkMap<PkString, PkVariant> metaDataForStorage(const PkString &storageLocation) const;

    /**
     * @brief setMetaDataForStorage
     * @param storage
     * @param map
     */
    void setMetaDataForStorage(const PkString &storageLocation, PkMap<PkString, PkVariant> map) const;

    /**
     * Loads all the resources required by \p resource into the cache
     *
     * loadRequiredResources() also loads embedded resources and adds them
     * into the database.
     */
    void loadRequiredResources(KoResourceSP resource);

    /**
     * @brief tagForUrl create a tag from the database
     * @param tagUrl the url
     * @return a complete tag with all translated names and comments.
     */
    KisTagSP tagForUrl(const PkString &tagUrl, const PkString resourceType);

    /**
     * @brief tagForUrlNoCache create a tag from the database, don't use cache
     * @param tagUrl url of the tag
     * @param resourceType resource type of the tag
     * @return
     */
    static KisTagSP tagForUrlNoCache(const PkString &tagUrl, const PkString resourceType);

    KisResourceLocator();
    KisResourceLocator(const KisResourceLocator&) = delete;
    KisResourceLocator &operator=(const KisResourceLocator&) = delete;

    enum class InitializationStatus {
        Unknown,      // We don't know whether Krita has run on this system for this resource location yet
        Initialized,  // Everything is ready to start synchronizing the database
        FirstRun,     // Krita hasn't run for this resource location yet
        FirstUpdate,  // Krita was installed, but it's a version from before the resource locator existed, only user-defined resources are present
        Updating      // Krita is updating from an older version with resource locator
    };

    LocatorError firstTimeInstallation(InitializationStatus initializationStatus, const PkString &installationResourcesLocation);

    // Synchronize on restarting Krita to see whether the user has added any storages or resources to the resources location
    bool synchronizeDb();

    void findStorages();
    PkList<KisResourceStorageSP> storages() const;

    KisResourceStorageSP storageByLocation(const PkString &location) const;
    KisResourceStorageSP folderStorage() const;
    KisResourceStorageSP memoryStorage() const;
    KisResourceStorageSP fontStorage() const;

    struct ResourceStorage {
        PkString storageLocation;
        PkString resourceType;
        PkString resourceFileName;
     };

    friend class KisMyPaintPaintOpPreset;

    ResourceStorage getResourceStorage(int resourceId) const;
    PkString makeStorageLocationAbsolute(PkString storageLocation) const;
    PkString makeStorageLocationRelative(PkString location) const;

    class Private;
    PkScopedPointer<Private> d;
};

#endif // KISRESOURCELOCATOR_H
