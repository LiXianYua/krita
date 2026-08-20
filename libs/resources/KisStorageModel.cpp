/*
 * SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2023 L. E. Segovia <amy@amyspark.me>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisStorageModel.h"

#include <PkSqlQuery.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "KisResourceLocator.h"
#include "KisResourceThumbnailCache.h"
#include "KisResourceThumbnailCodec.h"
#include "KoResourcePaths.h"

namespace fs = std::filesystem;

namespace
{

PkString fromPath(const fs::path &path)
{
    const std::string value = path.u8string();
    return PkString::PkFromUtf8(value.data(), static_cast<int>(value.size()));
}

fs::path toPath(const PkString &path)
{
    return fs::u8path(path.PkToUtf8());
}

PkString intString(int value)
{
    const std::string text = std::to_string(value);
    return PkString::PkFromUtf8(text.data(), static_cast<int>(text.size()));
}

fs::path unusedPath(const fs::path &directory, const fs::path &filename)
{
    fs::path candidate = directory / filename;
    std::error_code error;
    if (!fs::exists(candidate, error)) {
        return candidate;
    }

    const std::string stem = filename.stem().u8string();
    const std::string extension = filename.extension().u8string();
    for (int version = 1; version < 1000000; ++version) {
        candidate = directory / fs::u8path(
            stem + "_" + std::to_string(version) + extension);
        error.clear();
        if (!fs::exists(candidate, error)) {
            return candidate;
        }
    }
    return fs::path();
}

bool writeStorageFile(const fs::path &source,
                      const fs::path &destination,
                      const PkByteArray &data)
{
    std::error_code error;
    fs::create_directories(destination.parent_path(), error);
    if (error) {
        return false;
    }

    fs::path temporary = destination;
    temporary += ".importing";
    for (int suffix = 0; fs::exists(temporary, error) && suffix < 1000; ++suffix) {
        temporary = destination;
        temporary += ".importing." + std::to_string(suffix + 1);
    }

    bool wrote = false;
    if (!data.isEmpty()) {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(data.constData(), data.size());
        output.flush();
        wrote = output.good();
    } else {
        error.clear();
        wrote = fs::copy_file(source,
                              temporary,
                              fs::copy_options::overwrite_existing,
                              error) && !error;
    }
    if (!wrote) {
        error.clear();
        fs::remove(temporary, error);
        return false;
    }

    error.clear();
    if (fs::exists(destination, error)) {
        fs::remove(temporary, error);
        return false;
    }
    error.clear();
    fs::rename(temporary, destination, error);
    if (error) {
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

} // namespace

struct KisStorageModel::Private
{
    PkVector<KisStorageRecord> records;
};

KisStorageModel::KisStorageModel(PkObject *parent)
    : PkObject(parent)
    , d(new Private)
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    if (locator) {
        PkObject::connect(locator,
                          &KisResourceLocator::storageAdded,
                          this,
                          &KisStorageModel::addStorage);
        PkObject::connect(locator,
                          &KisResourceLocator::storageRemoved,
                          this,
                          &KisStorageModel::removeStorage);
        PkObject::connect(locator,
                          &KisResourceLocator::storageResynchronized,
                          this,
                          &KisStorageModel::storageResynchronized);
        PkObject::connect(locator,
                          &KisResourceLocator::storagesBulkSynchronizationFinished,
                          this,
                          &KisStorageModel::slotStoragesBulkSynchronizationFinished);
    }
    refresh();
}

KisStorageModel::~KisStorageModel()
{
    delete d;
}

KisStorageModel *KisStorageModel::instance()
{
    static KisStorageModel model;
    return &model;
}

PkVector<KisStorageRecord> KisStorageModel::storages() const
{
    return d->records;
}

KisResourceStorageSP KisStorageModel::storageForId(int storageId) const
{
    KisResourceLocator *locator = KisResourceLocator::instance();
    if (!locator) {
        return KisResourceStorageSP();
    }
    for (const KisStorageRecord &record : d->records) {
        if (record.id == storageId) {
            return locator->storageByLocation(
                locator->makeStorageLocationAbsolute(record.location));
        }
    }
    return KisResourceStorageSP();
}

bool KisStorageModel::setStorageActive(int storageId, bool active)
{
    PkString location;
    for (const KisStorageRecord &record : d->records) {
        if (record.id == storageId) {
            location = record.location;
            break;
        }
    }
    if (location.isEmpty()) {
        return false;
    }

    PkSqlQuery query;
    if (!query.prepare(PkString(
            "UPDATE storages SET active = :active WHERE id = :id"))) {
        return false;
    }
    query.bindValue(PkString(":active"), PkVariant(active));
    query.bindValue(PkString(":id"), PkVariant(storageId));
    if (!query.exec()) {
        return false;
    }
    refresh();
    if (active) {
        storageEnabled(location);
    } else {
        storageDisabled(location);
    }
    return true;
}

bool KisStorageModel::importStorage(const PkString &filename,
                                    StorageImportOption importOption) const
{
    return importStorageInternal(filename, importOption, false, PkByteArray());
}

bool KisStorageModel::importStorageData(const PkString &filename,
                                        StorageImportOption importOption,
                                        const PkByteArray &data) const
{
    return !data.isEmpty() &&
        importStorageInternal(filename, importOption, false, data);
}

bool KisStorageModel::canImportStorage(const PkString &filename) const
{
    return importStorageInternal(filename, None, true, PkByteArray());
}

bool KisStorageModel::importStorageInternal(const PkString &filename,
                                            StorageImportOption importOption,
                                            bool dryRun,
                                            const PkByteArray &data)
{
    const fs::path source = toPath(filename);
    fs::path destinationDirectory = toPath(KoResourcePaths::getAppDataLocation());
    fs::path destination = destinationDirectory / source.filename();

    std::error_code error;
    const bool sourceReadable = !data.isEmpty() ||
        (fs::is_regular_file(source, error) && !error);
    if (!sourceReadable || source.filename().empty()) {
        return false;
    }

    error.clear();
    if (fs::exists(destination, error)) {
        if (importOption == Rename) {
            destination = unusedPath(destinationDirectory, source.filename());
            if (destination.empty()) {
                return false;
            }
        } else {
            return false;
        }
    }

    if (dryRun) {
        return true;
    }
    if (!writeStorageFile(source, destination, data)) {
        return false;
    }

    KisResourceStorageSP storage = KisResourceStorageSP::create(fromPath(destination));
    KisResourceLocator *locator = KisResourceLocator::instance();
    return storage && storage->valid() && locator &&
        locator->addStorage(fromPath(destination), storage);
}

void KisStorageModel::storageEnabled(const PkString &storage)
{
    activateSignal<const PkString &>(this,
                                     PkMemberFnKey::from(&KisStorageModel::storageEnabled),
                                     storage);
}

void KisStorageModel::storageDisabled(const PkString &storage)
{
    activateSignal<const PkString &>(this,
                                     PkMemberFnKey::from(&KisStorageModel::storageDisabled),
                                     storage);
}

void KisStorageModel::storageResynchronized(const PkString &storage, bool bulk)
{
    if (!bulk) {
        refresh();
    }
    activateSignal<const PkString &, bool>(
        this,
        PkMemberFnKey::from(&KisStorageModel::storageResynchronized),
        storage,
        bulk);
}

void KisStorageModel::storagesBulkSynchronizationFinished()
{
    activateSignal<>(
        this,
        PkMemberFnKey::from(&KisStorageModel::storagesBulkSynchronizationFinished));
}

void KisStorageModel::addStorage(const PkString &location)
{
    (void)location;
    refresh();
}

void KisStorageModel::removeStorage(const PkString &location)
{
    (void)location;
    refresh();
}

void KisStorageModel::slotStoragesBulkSynchronizationFinished()
{
    refresh();
    storagesBulkSynchronizationFinished();
}

bool KisStorageModel::refresh()
{
    PkSqlQuery query;
    if (!query.exec(PkString(
            "SELECT storages.id AS id, storage_types.name AS storage_type, "
            "storages.location AS location, storages.timestamp AS timestamp, "
            "storages.pre_installed AS pre_installed, storages.active AS active, "
            "storages.thumbnail AS thumbnail "
            "FROM storages "
            "JOIN storage_types ON storages.storage_type_id = storage_types.id "
            "ORDER BY storages.id"))) {
        return false;
    }

    PkVector<KisStorageRecord> replacement;
    KisResourceLocator *locator = KisResourceLocator::instance();
    KisResourceThumbnailCache *cache = KisResourceThumbnailCache::instance();
    while (query.next()) {
        KisStorageRecord record;
        record.id = query.value(PkString("id")).toInt();
        record.storageType = query.value(PkString("storage_type")).toString();
        record.location = query.value(PkString("location")).toString();
        record.timestamp = query.value(PkString("timestamp")).toLongLong();
        record.preInstalled = query.value(PkString("pre_installed")).toBool();
        record.active = query.value(PkString("active")).toBool();
        if (locator) {
            record.metaData = locator->metaDataForStorage(record.location);
        }
        record.displayName = record.location;
        PkString name = record.metaData.value(KisResourceStorage::s_meta_name).toString();
        if (name.isEmpty()) {
            name = record.metaData.value(KisResourceStorage::s_meta_title).toString();
        }
        if (!name.isEmpty()) {
            record.displayName = name;
        }

        if (cache) {
            record.thumbnail = cache->originalImage(record.location,
                                                     record.storageType,
                                                     intString(record.id));
        }
        if (record.thumbnail.isNull()) {
            record.thumbnail = KisResourceThumbnailCodec::decodePng(
                query.value(PkString("thumbnail")).toByteArray());
            if (cache && !record.thumbnail.isNull()) {
                cache->insert(record.location,
                              record.storageType,
                              intString(record.id),
                              record.thumbnail);
            }
        }
        replacement.append(record);
    }
    d->records = replacement;
    return true;
}
