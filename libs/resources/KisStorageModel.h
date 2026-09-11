/*
 * SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2023 L. E. Segovia <amy@amyspark.me>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISSTORAGEMODEL_H
#define KISSTORAGEMODEL_H

#include <PkImage.h>
#include <PkMap.h>
#include <PkObject.h>
#include <PkString.h>
#include <PkVariant.h>
#include <PkVector.h>

#include "KisResourceStorage.h"
#include "kritaresources_export.h"

struct KRITARESOURCES_EXPORT KisStorageRecord
{
    int id = -1;
    PkString storageType;
    PkString location;
    long long timestamp = 0;
    bool preInstalled = false;
    bool active = false;
    PkImage thumbnail;
    PkString displayName;
    PkMap<PkString, PkVariant> metaData;
};

/** Process-wide data snapshot of registered resource storages. */
class KRITARESOURCES_EXPORT KisStorageModel : public PkObject
{
public:
    enum StorageImportOption {
        None,
        Overwrite,
        Rename,
    };

    explicit KisStorageModel(PkObject *parent = nullptr);
    ~KisStorageModel() override;

    static KisStorageModel *instance();

    PkVector<KisStorageRecord> storages() const;

    /**
     * Return the record registered under \p storageId without reading its metadata
     * (storages() is the path that populates it). An unknown id yields a
     * default-constructed record, whose id is -1 and therefore never a real one.
     */
    KisStorageRecord recordForId(int storageId) const;
    KisResourceStorageSP storageForId(int storageId) const;
    bool setStorageActive(int storageId, bool active);

    bool importStorage(const PkString &filename,
                       StorageImportOption importOption) const;
    bool importStorageData(const PkString &filename,
                           StorageImportOption importOption,
                           const PkByteArray &data) const;
    bool canImportStorage(const PkString &filename) const;

    void storageEnabled(const PkString &storage);
    void storageDisabled(const PkString &storage);
    void storageResynchronized(const PkString &storage, bool bulk);
    void storagesBulkSynchronizationFinished();

private:
    static bool importStorageInternal(const PkString &filename,
                                      StorageImportOption importOption,
                                      bool dryRun,
                                      const PkByteArray &data);

    /// Resolves the live metadata of \p record from its storage.
    static void populateMetaData(KisStorageRecord &record);

    void addStorage(const PkString &location);
    void removeStorage(const PkString &location);
    void slotStoragesBulkSynchronizationFinished();
    bool refresh();

    struct Private;
    Private *const d;
};

#endif // KISSTORAGEMODEL_H
