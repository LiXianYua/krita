/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisResourceLocator.h"

#include <PkDateTime.h>
#include <PkFileStream.h>
#include <PkHash.h>
#include <PkList.h>
#include <PkMap.h>
#include <PkMemoryStream.h>
#include <PkSqlQuery.h>
#include <PkVariant.h>
#include <PkVector.h>

#include <KritaVersionWrapper.h>
#include <KisMimeDatabase.h>
#include <kis_assert.h>
#include <kis_debug.h>

#include "KoResourcePaths.h"
#include "KisResourceStorage.h"
#include "KisResourceCacheDb.h"
#include "KisResourceLoaderRegistry.h"
#include "KisMemoryStorage.h"
#include <KisGlobalResourcesInterface.h>
#include <KoMD5Generator.h>
#include <KoResourceLoadResult.h>
#include <KisResourceThumbnailCache.h>
#include "PkResourceStorageDesktop.h"

#include "ResourceDebug.h"

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

PkString fromPath(const fs::path &path)
{
    const std::string value = path.u8string();
    return PkString::PkFromUtf8(value.data(), static_cast<int>(value.size()));
}

fs::path toPath(const PkString &path)
{
    return fs::u8path(path.PkToUtf8());
}

bool pathWritable(const fs::path &path)
{
#ifdef _WIN32
    return ::_waccess(path.wstring().c_str(), 2) == 0;
#else
    return ::access(path.c_str(), W_OK) == 0;
#endif
}

PkString fileName(const PkString &path)
{
    return fromPath(toPath(path).filename());
}

PkString suffix(const PkString &path)
{
    std::string extension = toPath(path).extension().u8string();
    if (!extension.empty() && extension.front() == '.') extension.erase(extension.begin());
    return PkString::PkFromUtf8(extension.data(), static_cast<int>(extension.size()));
}

std::string lowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool endsWithAsciiCaseInsensitive(const PkString &value, const char *ending)
{
    const std::string text = lowerAscii(value.PkToUtf8());
    const std::string tail = lowerAscii(ending);
    return text.size() >= tail.size() &&
           text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

PkByteArray readAll(PkStream *stream)
{
    if (!stream || !stream->isOpen() || !stream->isReadable()) return {};
    std::vector<char> bytes;
    char chunk[16384];
    for (;;) {
        const PkStream::pk_int64 read = stream->read(chunk, sizeof(chunk));
        if (read < 0) return {};
        if (read == 0) break;
        bytes.insert(bytes.end(), chunk, chunk + read);
    }
    return PkByteArray(bytes.data(), static_cast<int>(bytes.size()));
}

bool loadMemoryStream(PkMemoryStream &stream, const PkByteArray &bytes)
{
    if (!stream.open(static_cast<PkStream::OpenMode>(PkStream::ReadWrite | PkStream::Truncate))) return false;
    if (stream.write(bytes.constData(), bytes.size()) != bytes.size()) return false;
    stream.close();
    return stream.open(PkStream::ReadOnly);
}

struct ResourceVersion
{
    int major = 0;
    int minor = 0;
    int patch = 0;

    static ResourceVersion fromString(const PkString &text)
    {
        ResourceVersion result;
        if (std::sscanf(text.PkToUtf8().c_str(), "%d.%d.%d", &result.major, &result.minor, &result.patch) < 2) {
            return {};
        }
        return result;
    }

    friend bool operator<(const ResourceVersion &a, const ResourceVersion &b)
    {
        return std::tie(a.major, a.minor, a.patch) < std::tie(b.major, b.minor, b.patch);
    }
    friend bool operator>(const ResourceVersion &a, const ResourceVersion &b) { return b < a; }
};

PkString replaceSpaces(const PkString &name)
{
    std::string value = name.PkToUtf8();
    std::replace(value.begin(), value.end(), ' ', '_');
    return PkString::PkFromUtf8(value.data(), static_cast<int>(value.size()));
}

PkString removeBasePath(const PkString &location, const PkString &base)
{
    return location.startsWith(base) ? location.mid(base.size()) : location;
}

struct LocatorSingletonSlot
{
    enum class State {
        Available,
        ShuttingDown,
        Shutdown
    };

    std::mutex mutex;
    std::condition_variable condition;
    State state = State::Available;
    std::thread::id shutdownThread;
    KisResourceLocator *instance = nullptr;
};

LocatorSingletonSlot &locatorSingletonSlot()
{
    // The slot deliberately outlives process teardown. Only shutdown() owns
    // destruction of the locator itself, so no atexit order can race plugin
    // and storage destruction.
    static LocatorSingletonSlot *slot = new LocatorSingletonSlot;
    return *slot;
}

PkString deduplicateEmbeddedFileName(const PkString &proposedFileName,
                                     const std::function<bool(PkString)> &fileAllowed)
{
    const std::string separator = "_embedded_";
    const std::string original = toPath(proposedFileName).filename().u8string();
    const std::size_t firstDot = original.find('.');
    std::string baseName = original.substr(0, firstDot);
    std::string completeSuffix = firstDot == std::string::npos
        ? std::string()
        : original.substr(firstDot + 1);

    // Exact equivalent of KritaUtils::deduplicateFileName's separator
    // recognition: the separator must be left of the first dot, followed by
    // one or more decimal digits and then either end-of-name or a non-empty
    // complete suffix.
    const std::size_t separatorSearchEnd = firstDot == std::string::npos
        ? original.size()
        : firstDot;
    std::size_t separatorPos = original.rfind(separator, separatorSearchEnd);
    while (separatorPos != std::string::npos && separatorPos > 0) {
        const std::size_t digitsBegin = separatorPos + separator.size();
        std::size_t digitsEnd = digitsBegin;
        while (digitsEnd < original.size() &&
               std::isdigit(static_cast<unsigned char>(original[digitsEnd]))) {
            ++digitsEnd;
        }
        const bool hasDigits = digitsEnd > digitsBegin;
        const bool validTail = digitsEnd == original.size() ||
            (original[digitsEnd] == '.' && digitsEnd + 1 < original.size());
        if (hasDigits && validTail) {
            baseName = original.substr(0, separatorPos);
            completeSuffix = digitsEnd == original.size()
                ? std::string()
                : original.substr(digitsEnd + 1);
            break;
        }
        separatorPos = original.rfind(separator, separatorPos - 1);
    }

    PkString candidate = PkString::PkFromUtf8(original.data(), static_cast<int>(original.size()));
    for (int counter = 0; !fileAllowed(candidate); ++counter) {
        std::string next = baseName + separator + std::to_string(counter);
        if (!completeSuffix.empty()) {
            next += "." + completeSuffix;
        }
        candidate = PkString::PkFromUtf8(next.data(), static_cast<int>(next.size()));
    }
    return candidate;
}

bool writeFile(const fs::path &path, const char *data, std::size_t size)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(data, static_cast<std::streamsize>(size));
    out.flush();
    return out.good();
}
}

const PkString KisResourceLocator::resourceLocationKey {"ResourceDirectory"};

class KisResourceLocator::Private {
public:
    PkString resourceLocation;
    PkMap<PkString, KisResourceStorageSP> storages;
    PkMap<std::pair<PkString, PkString>, KoResourceSP> resourceCache;
    PkMap<std::pair<PkString, PkString>, KisTagSP> tagCache;
    PkStringList errorMessages;

    KisResourceStorageSP safeGetStorage(const PkString &storageLocation) {
        /**
         * When using a []-operator on a map object, a new (null) element
         * may accidentially be created, if no such element is present.
         * Hence we should be more careful with doing that.
         */

        if (!storages.contains(storageLocation)) {
            qWarning() << "WARNING: KisResourceLocator: failed to find a storage with location" << storageLocation;
            return nullptr;
        }

        return storages[storageLocation];
    }
};

KisResourceLocator::KisResourceLocator()
    : PkObject(nullptr)
    , d(new Private())
{
}

KisResourceLocator *KisResourceLocator::instance()
{
    LocatorSingletonSlot &slot = locatorSingletonSlot();
    std::lock_guard<std::mutex> lock(slot.mutex);
    if (slot.state != LocatorSingletonSlot::State::Available) {
        return nullptr;
    }
    if (!slot.instance) {
        slot.instance = new KisResourceLocator;
    }
    return slot.instance;
}

KisResourceLocator *KisResourceLocator::existingInstance()
{
    LocatorSingletonSlot &slot = locatorSingletonSlot();
    std::lock_guard<std::mutex> lock(slot.mutex);
    return slot.state == LocatorSingletonSlot::State::Available ? slot.instance : nullptr;
}

void KisResourceLocator::shutdown()
{
    LocatorSingletonSlot &slot = locatorSingletonSlot();
    KisResourceLocator *oldInstance = nullptr;
    {
        std::unique_lock<std::mutex> lock(slot.mutex);
        if (slot.state == LocatorSingletonSlot::State::Shutdown) {
            return;
        }
        if (slot.state == LocatorSingletonSlot::State::ShuttingDown) {
            if (slot.shutdownThread == std::this_thread::get_id()) {
                return;
            }
            slot.condition.wait(lock, [&slot] {
                return slot.state == LocatorSingletonSlot::State::Shutdown;
            });
            return;
        }

        slot.state = LocatorSingletonSlot::State::ShuttingDown;
        slot.shutdownThread = std::this_thread::get_id();
        oldInstance = slot.instance;
        slot.instance = nullptr;
    }
    delete oldInstance;

    {
        std::lock_guard<std::mutex> lock(slot.mutex);
        slot.state = LocatorSingletonSlot::State::Shutdown;
        slot.shutdownThread = std::thread::id();
    }
    slot.condition.notify_all();
}

KisResourceLocator::~KisResourceLocator()
{
}

void KisResourceLocator::progressMessage(const PkString &message)
{
    activateSignal<const PkString &>(this, PkMemberFnKey::from(&KisResourceLocator::progressMessage), message);
}

void KisResourceLocator::storageAdded(const PkString &location)
{
    activateSignal<const PkString &>(this, PkMemberFnKey::from(&KisResourceLocator::storageAdded), location);
}

void KisResourceLocator::storageRemoved(const PkString &location)
{
    activateSignal<const PkString &>(this, PkMemberFnKey::from(&KisResourceLocator::storageRemoved), location);
}

void KisResourceLocator::beginExternalResourceImport(const PkString &resourceType, int numResources)
{
    activateSignal<const PkString &, int>(this,
                                         PkMemberFnKey::from(&KisResourceLocator::beginExternalResourceImport),
                                         resourceType,
                                         numResources);
}

void KisResourceLocator::endExternalResourceImport(const PkString &resourceType)
{
    activateSignal<const PkString &>(this,
                                     PkMemberFnKey::from(&KisResourceLocator::endExternalResourceImport),
                                     resourceType);
}

void KisResourceLocator::beginExternalResourceRemove(const PkString &resourceType,
                                                     const PkVector<int> resourceIds)
{
    activateSignal<const PkString &, PkVector<int>>(this,
                                                    PkMemberFnKey::from(&KisResourceLocator::beginExternalResourceRemove),
                                                    resourceType,
                                                    resourceIds);
}

void KisResourceLocator::endExternalResourceRemove(const PkString &resourceType)
{
    activateSignal<const PkString &>(this,
                                     PkMemberFnKey::from(&KisResourceLocator::endExternalResourceRemove),
                                     resourceType);
}

void KisResourceLocator::resourceActiveStateChanged(const PkString &resourceType, int resourceId)
{
    activateSignal<const PkString &, int>(this,
                                         PkMemberFnKey::from(&KisResourceLocator::resourceActiveStateChanged),
                                         resourceType,
                                         resourceId);
}

void KisResourceLocator::storageResynchronized(const PkString &storage, bool isBulkResynchronization)
{
    activateSignal<const PkString &, bool>(this,
                                          PkMemberFnKey::from(&KisResourceLocator::storageResynchronized),
                                          storage,
                                          isBulkResynchronization);
}

void KisResourceLocator::storagesBulkSynchronizationFinished()
{
    activateSignal<>(this,
                     PkMemberFnKey::from(&KisResourceLocator::storagesBulkSynchronizationFinished));
}

KisResourceLocator::LocatorError KisResourceLocator::initialize(const PkString &installationResourcesLocation)
{
    InitializationStatus initializationStatus = InitializationStatus::Unknown;

    d->resourceLocation = KoResourcePaths::getAppDataLocation();

    if (!endsWithAsciiCaseInsensitive(d->resourceLocation, "/") &&
        !endsWithAsciiCaseInsensitive(d->resourceLocation, "\\")) {
        d->resourceLocation += "/";
    }

    std::error_code ec;
    const fs::path resourcePath = toPath(d->resourceLocation);
    if (!fs::exists(resourcePath, ec)) {
        if (!fs::create_directories(resourcePath, ec) || ec) {
            d->errorMessages.append(PkString("1. Could not create the resource location at %1.").arg(d->resourceLocation));
            return LocatorError::CannotCreateLocation;
        }
        initializationStatus = InitializationStatus::FirstRun;
    }

    if (!pathWritable(resourcePath)) {
        d->errorMessages.append(PkString("2. The resource location at %1 is not writable.").arg(d->resourceLocation));
        return LocatorError::LocationReadOnly;
    }

    // Check whether we're updating from an older version
    if (initializationStatus != InitializationStatus::FirstRun) {
        std::ifstream versionFile(resourcePath / "KRITA_RESOURCE_VERSION", std::ios::binary);
        if (!versionFile) {
            initializationStatus = InitializationStatus::FirstUpdate;
        }
        else {
            const std::string bytes{std::istreambuf_iterator<char>(versionFile), std::istreambuf_iterator<char>()};
            const ResourceVersion resource_version = ResourceVersion::fromString(
                PkString::PkFromUtf8(bytes.data(), static_cast<int>(bytes.size())));
            const ResourceVersion krita_version = ResourceVersion::fromString(KritaVersionWrapper::versionString());
            if (krita_version > resource_version) {
                initializationStatus = InitializationStatus::Updating;
            }
            else {
                initializationStatus = InitializationStatus::Initialized;
            }
        }
    }

    if (initializationStatus != InitializationStatus::Initialized) {
        KisResourceLocator::LocatorError res = firstTimeInstallation(initializationStatus, installationResourcesLocation);
        if (res != LocatorError::Ok) {
            return res;
        }
        initializationStatus = InitializationStatus::Initialized;
    }

    if (!synchronizeDb()) {
        return LocatorError::CannotSynchronizeDb;
    }

    return LocatorError::Ok;
}

PkStringList KisResourceLocator::errorMessages() const
{
    return d->errorMessages;
}

PkString KisResourceLocator::resourceLocationBase() const
{
    return d->resourceLocation;
}

bool KisResourceLocator::resourceCached(PkString storageLocation, const PkString &resourceType, const PkString &filename) const
{
    storageLocation = makeStorageLocationAbsolute(storageLocation);
    std::pair<PkString, PkString> key = std::pair<PkString, PkString> (storageLocation, resourceType + "/" + filename);

    return d->resourceCache.contains(key);
}

void KisResourceLocator::loadRequiredResources(KoResourceSP resource)
{
    auto loadResourcesGroup =
            [this, parentResource = resource] (PkVector<KoResourceLoadResult> resources,
            const PkString &resourceGroup) {

        for (KoResourceLoadResult res : resources) {
            switch (res.type())
            {
            case KoResourceLoadResult::ExistingResource:
                KIS_SAFE_ASSERT_RECOVER_NOOP(res.resource()->resourceId() >= 0);
                break;
            case KoResourceLoadResult::EmbeddedResource: {
                KoResourceSignature sig = res.embeddedResource().signature();
                const PkByteArray data = res.embeddedResource().data();
                PkMemoryStream buffer;
                if (!loadMemoryStream(buffer, data)) {
                    qWarning() << "Failed to buffer" << resourceGroup << "resource:" << sig;
                    break;
                }
                importResourceDeduplicateFileName(sig.type, sig.filename, &buffer, "memory");
                break;
            }
            case KoResourceLoadResult::FailedLink:
                qWarning() << "Failed to load" << resourceGroup << "resource:" << res.signature();
                break;
            }
        }
    };

    /**
     * First load the side-loaded resources, since they may be linked
     * by the linked resources.
     */
    loadResourcesGroup(resource->takeSideLoadedResources(KisGlobalResourcesInterface::instance()), "side-loaded");

    /**
     * Now load the linked resources
     */
    loadResourcesGroup(resource->requiredResources(KisGlobalResourcesInterface::instance()), "linked");
}

KisTagSP KisResourceLocator::tagForUrl(const PkString &tagUrl, const PkString resourceType)
{
    if (d->tagCache.contains(std::pair<PkString, PkString>(resourceType, tagUrl))) {
        return d->tagCache[std::pair<PkString, PkString>(resourceType, tagUrl)];
    }

    KisTagSP tag = tagForUrlNoCache(tagUrl, resourceType);

    if (tag && tag->valid()) {
        d->tagCache[std::pair<PkString, PkString>(resourceType, tagUrl)] = tag;
    }

    return tag;
}

KisTagSP KisResourceLocator::tagForUrlNoCache(const PkString &tagUrl, const PkString resourceType)
{
    PkSqlQuery query;
    bool r = query.prepare("SELECT tags.id\n"
                           ",      tags.url\n"
                           ",      tags.active\n"
                           ",      tags.name\n"
                           ",      tags.comment\n"
                           ",      tags.filename\n"
                           ",      resource_types.name as resource_type\n"
                           ",      resource_types.id\n"
                           "FROM   tags\n"
                           ",      resource_types\n"
                           "WHERE  tags.resource_type_id = resource_types.id\n"
                           "AND    resource_types.name = :resource_type\n"
                           "AND    tags.url = :tag_url\n");

    if (!r) {
        qWarning() << "Could not prepare KisResourceLocator::tagForUrl query" << query.lastError();
        return KisTagSP();
    }

    query.bindValue(":resource_type", resourceType);
    query.bindValue(":tag_url", tagUrl);

    r = query.exec();
    if (!r) {
        qWarning() << "Could not execute KisResourceLocator::tagForUrl query" << query.lastError() << query.boundValues();
        return KisTagSP();
    }

    r = query.first();
    if (!r) {
        return KisTagSP();
    }

    KisTagSP tag(new KisTag());

    int tagId = query.value("tags.id").toInt();
    int resourceTypeId = query.value("resource_types.id").toInt();

    tag->setUrl(query.value("url").toString());
    tag->setResourceType(resourceType);
    tag->setId(query.value("id").toInt());
    tag->setActive(query.value("active").toBool());
    tag->setName(query.value("name").toString());
    tag->setComment(query.value("comment").toString());
    tag->setFilename(query.value("filename").toString());
    tag->setValid(true);


    PkMap<PkString, PkString> names;
    PkMap<PkString, PkString> comments;

    r = query.prepare("SELECT language\n"
                      ",      name\n"
                      ",      comment\n"
                      "FROM   tag_translations\n"
                      "WHERE  tag_id = :id");

    if (!r) {
        qWarning() << "Could not prepare KisResourceLocator::tagForUrl translation query" << query.lastError();
    }

    query.bindValue(":id", tag->id());

    if (!query.exec()) {
        qWarning() << "Could not execute KisResourceLocator::tagForUrl translation query" << query.lastError();
    }

    while (query.next()) {
        names[query.value(0).toString()] = query.value(1).toString();
        comments[query.value(0).toString()] = query.value(2).toString();
    }

    tag->setNames(names);
    tag->setComments(comments);

    PkSqlQuery defaultResourcesQuery;

    if (!defaultResourcesQuery.prepare("SELECT resources.filename\n"
                                       "FROM   resources\n"
                                       ",      resource_tags\n"
                                       "WHERE  resource_tags.tag_id = :tag_id\n"
                                       "AND    resources.resource_type_id = :type_id\n"
                                       "AND    resource_tags.resource_id = resources.id\n"
                                       "AND    resource_tags.active = 1\n")) {
        qWarning() << "Could not prepare resource/tag query" << defaultResourcesQuery.lastError();
    }

    defaultResourcesQuery.bindValue(":tag_id", tagId);
    defaultResourcesQuery.bindValue(":type_id", resourceTypeId);

    if (!defaultResourcesQuery.exec()) {
        qWarning() << "Could not execute resource/tag query" << defaultResourcesQuery.lastError();
    }

    PkStringList resourceFileNames;

    while (defaultResourcesQuery.next()) {
        resourceFileNames << defaultResourcesQuery.value("resources.filename").toString();
    }

    tag->setDefaultResources(resourceFileNames);

    return tag;
}


KoResourceSP KisResourceLocator::resource(PkString storageLocation, const PkString &resourceType, const PkString &filename)
{
    storageLocation = makeStorageLocationAbsolute(storageLocation);

    std::pair<PkString, PkString> key = std::pair<PkString, PkString> (storageLocation, resourceType + "/" + filename);

    KoResourceSP resource;
    if (d->resourceCache.contains(key)) {
        resource = d->resourceCache[key];
    }
    else {
        KisResourceStorageSP storage = d->safeGetStorage(storageLocation);
        if (!storage) {
            return 0;
        }

        resource = storage->resource(resourceType + "/" + filename);

        if (resource) {
            d->resourceCache[key] = resource;
            // load all the embedded resources into temporary "memory" storage
            loadRequiredResources(resource);
        }
    }

    if (!resource) {
        qWarning() << "KoResourceSP KisResourceLocator::resource" << storageLocation << resourceType << filename << "was not found";
        return 0;
    }

    resource->setStorageLocation(storageLocation);
    KIS_ASSERT(!resource->storageLocation().isEmpty());

    if (resource->resourceId() < 0 || resource->version() < 0) {
        PkSqlQuery q;
        if (!q.prepare("SELECT resources.id\n"
                       ",      versioned_resources.version as version\n"
                       ",      versioned_resources.md5sum as md5sum\n"
                       ",      resources.name\n"
                       ",      resources.status\n"
                       "FROM   resources\n"
                       ",      storages\n"
                       ",      resource_types\n"
                       ",      versioned_resources\n"
                       "WHERE  storages.id = resources.storage_id\n"
                       "AND    storages.location = :storage_location\n"
                       "AND    resource_types.id = resources.resource_type_id\n"
                       "AND    resource_types.name = :resource_type\n"
                       "AND    resources.filename  = :filename\n"
                       "AND    versioned_resources.resource_id = resources.id\n"
                       "AND    versioned_resources.version = (SELECT MAX(version) FROM versioned_resources WHERE versioned_resources.resource_id = resources.id)")) {
            qWarning() << "Could not prepare id/version query" << q.lastError();

        }

        q.bindValue(":storage_location", makeStorageLocationRelative(storageLocation));
        q.bindValue(":resource_type", resourceType);
        q.bindValue(":filename", filename);

        if (!q.exec()) {
            qWarning() << "Could not execute id/version query" << q.lastError() << q.boundValues();
        }

        if (!q.first()) {
            qWarning() << "Could not find the resource in the database" << storageLocation << resourceType << filename;
        }

        resource->setResourceId(q.value(0).toInt());
        KIS_ASSERT(resource->resourceId() >= 0);

        resource->setVersion(q.value(1).toInt());
        KIS_ASSERT(resource->version() >= 0);

        resource->setMD5Sum(q.value(2).toString());
        KIS_ASSERT(!resource->md5Sum().isEmpty());

        resource->setActive(q.value(4).toBool());

        // To override resources that use the filename for the name, which is versioned, and we don't want the version number in the name
        resource->setName(q.value(3).toString());;
    }

    if (!resource) {
        qWarning() << "Could not find resource" << resourceType + "/" + filename;
        return 0;
    }

    return resource;
}

KoResourceSP KisResourceLocator::resourceForId(int resourceId)
{
    ResourceStorage rs = getResourceStorage(resourceId);
    KoResourceSP r = resource(rs.storageLocation, rs.resourceType, rs.resourceFileName);
    return r;
}

bool KisResourceLocator::setResourceActive(int resourceId, bool active)
{
    // First remove the resource from the cache
    ResourceStorage rs = getResourceStorage(resourceId);
    std::pair<PkString, PkString> key = std::pair<PkString, PkString> (rs.storageLocation, rs.resourceType + "/" + rs.resourceFileName);

    d->resourceCache.remove(key);
    if (!active) {
        KisResourceThumbnailCache::instance()->remove(key);
    }

    bool result = KisResourceCacheDb::setResourceActive(resourceId, active);

    resourceActiveStateChanged(rs.resourceType, resourceId);

    return result;
}

KoResourceSP KisResourceLocator::importResourceFromFile(const PkString &resourceType, const PkString &fileName, const bool allowOverwrite, const PkString &storageLocation)
{
    PkFileStream file(fileName);
    if (!file.open(PkStream::ReadOnly)) {
        qWarning() << "Could not open" << fileName << "for loading";
        return nullptr;
    }

    return importResource(resourceType, fileName, &file, allowOverwrite, storageLocation);
}

KoResourceSP KisResourceLocator::importResource(const PkString &resourceType, const PkString &fileName, PkStream *device, const bool allowOverwrite, const PkString &storageLocation)
{
    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(storageLocation));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, nullptr);

    const PkByteArray resourceData = readAll(device);
    KoResourceSP resource;

    {
        PkMemoryStream buf;
        if (!loadMemoryStream(buf, resourceData)) {
            qWarning() << "Could not buffer" << fileName << "for loading";
            return nullptr;
        }

        KisResourceLoaderBase *loader = KisResourceLoaderRegistry::instance()->loader(resourceType, KisMimeDatabase::mimeTypeForFile(fileName));

        if (!loader) {
            qWarning() << "Could not import" << fileName << ": resource doesn't load.";
            return nullptr;
        }

        resource = loader->load(::fileName(fileName), buf, KisGlobalResourcesInterface::instance());
    }

    if (!resource || !resource->valid()) {
        qWarning() << "Could not import" << fileName << ": resource doesn't load.";
        return nullptr;
    }

    const PkString md5 = KoMD5Generator::generateHash(resourceData);
    const PkString resourceUrl = resourceType + "/" + resource->filename();

    KoResourceSP existingResource = storage->resource(resourceUrl);

    if (existingResource) {
        const PkString existingResourceMd5Sum = storage->resourceMd5(resourceUrl);

        if (!allowOverwrite) {
            return nullptr;
        }

        if (existingResourceMd5Sum == md5 &&
            existingResource->filename() == resource->filename()) {

            /**
             * Make sure that this resource is the latest version of the
             * resource. Also, we cannot just return existingResource, because
             * it has uninitialized fields. It should go through the initialization
             * by the locator's caching system.
             */

            int existingResourceId = -1;
            bool r = KisResourceCacheDb::getResourceIdFromFilename(existingResource->filename(), resourceType, storageLocation, existingResourceId);

            if (r && existingResourceId > 0) {
                return resourceForId(existingResourceId);
            }
        }

        qWarning() << "A resource with the same filename but a different MD5 already exists in the storage" << resourceType << fileName << storageLocation;
        if (storageLocation == "") {
            qWarning() << "Proceeding with overwriting the existing resource...";
            // remove all versions of the resource from the resource folder
            PkStringList versionsLocations;

            // this resource has id -1, we need correct id
            int existingResourceId = -1;
            bool r = KisResourceCacheDb::getResourceIdFromVersionedFilename(existingResource->filename(), resourceType, storageLocation, existingResourceId);

            if (r && existingResourceId >= 0) {
                if (KisResourceCacheDb::getAllVersionsLocations(existingResourceId, versionsLocations)) {

                    for (int i = 0; i < versionsLocations.size(); i++) {
                        const fs::path versionPath = toPath(this->resourceLocationBase() + "/" + resourceType + "/" + versionsLocations[i]);
                        std::error_code ec;
                        if (fs::exists(versionPath, ec)) {
                            r = fs::remove(versionPath, ec) && !ec;
                            if (!r) {
                                qWarning() << "KisResourceLocator::importResourceFromFile: Removal of " << fromPath(versionPath)
                                           << "was requested, but it wasn't possible, something went wrong.";
                            }
                        } else {
                            qWarning() << "KisResourceLocator::importResourceFromFile: Removal of " << fromPath(versionPath)
                                       << "was requested, but it doesn't exist.";
                        }
                    }
                } else {
                    qWarning() << "KisResourceLocator::importResourceFromFile: Finding all locations for " << existingResourceId << "was requested, but it failed.";
                    return nullptr;
                }
            } else {
                qWarning() << "KisResourceLocator::importResourceFromFile: there is no resource file found in the location of " << storageLocation << resource->filename() << resourceType;
                return nullptr;
            }

            beginExternalResourceRemove(resourceType, {existingResourceId});

            // remove everything related to this resource from the database (remember about tags and versions!!!)
            r = KisResourceCacheDb::removeResourceCompletely(existingResourceId);

            {
                const PkString absoluteStorageLocation = makeStorageLocationAbsolute(resource->storageLocation());
                KisResourceThumbnailCache::instance()->remove(absoluteStorageLocation, resourceType, existingResource->filename());
            }

            endExternalResourceRemove(resourceType);

            if (!r) {
                qWarning() << "KisResourceLocator::importResourceFromFile: Removing resource with id " << existingResourceId << "completely from the database failed.";
                return nullptr;
            }

        } else {
            qWarning() << "KisResourceLocator::importResourceFromFile: Overwriting of the resource was denied, aborting import.";
            return nullptr;
        }
    }

    PkMemoryStream buf;
    if (!loadMemoryStream(buf, resourceData)) {
        return nullptr;
    }

    if (storage->importResource(resourceUrl, &buf)) {
        resource = storage->resource(resourceUrl);

        if (!resource) {
            qWarning() << "Could not retrieve imported resource from the storage" << resourceType << fileName << storageLocation;
            return nullptr;
        }

        resource->setStorageLocation(storageLocation);
        resource->setMD5Sum(storage->resourceMd5(resourceUrl));
        resource->setVersion(0);
        resource->setDirty(false);
        loadRequiredResources(resource);

        beginExternalResourceImport(resourceType, 1);

        // Insert into the database
        const bool result = KisResourceCacheDb::addResource(storage,
                                                storage->timeStampForResource(resourceType, resource->filename()),
                                                resource,
                                                resourceType);

        endExternalResourceImport(resourceType);

        if (!result) {
            return nullptr;
        }

        // resourceCaches use absolute locations
        const PkString absoluteStorageLocation = makeStorageLocationAbsolute(resource->storageLocation());
        const std::pair<PkString, PkString> key = {absoluteStorageLocation, resourceType + "/" + resource->filename()};
        // Add to the cache
        d->resourceCache[key] = resource;
        KisResourceThumbnailCache::instance()->insert(key, resource->thumbnail());

        return resource;
    }

    return nullptr;
}

namespace {
PkString findDeduplicatedFileName(const PkString &resourceType, const PkString &proposedFileName, KisResourceStorageSP storage)
{
    return deduplicateEmbeddedFileName(
        proposedFileName,
        [resourceType, storage](PkString candidate) {
            return !storage->resource(resourceType + "/" + candidate);
        });
}
}

KoResourceSP KisResourceLocator::importResourceDeduplicateFileName(const PkString &resourceType, const PkString &proposedFileName, PkStream *device, const PkString &storageLocation)
{
    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(storageLocation));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, nullptr);

    const PkString fileName = findDeduplicatedFileName(resourceType, proposedFileName, storage);
    return importResource(resourceType, fileName, device, false, storageLocation);
}

bool KisResourceLocator::addResourceDeduplicateFileName(const PkString &resourceType, const KoResourceSP resource, const PkString &storageLocation)
{
    // fix filename is missing
    if (resource->filename().isEmpty()) {
        resource->setFilename(replaceSpaces(resource->name()) + resource->defaultFileExtension());
    }

    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!resource->filename().isEmpty(), false);

    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(storageLocation));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, false);

    const PkString fileName = findDeduplicatedFileName(resourceType, resource->filename(), storage);
    resource->setFilename(fileName);
    return addResource(resourceType, resource, storageLocation);
}

bool KisResourceLocator::importWillOverwriteResource(const PkString &resourceType, const PkString &fileName, const PkString &storageLocation) const
{
    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(storageLocation));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, false);

    const PkString resourceUrl = resourceType + "/" + ::fileName(fileName);

    KoResourceSP existingResource = storage->resource(resourceUrl);

    return !existingResource.isNull();
}

bool KisResourceLocator::exportResource(KoResourceSP resource, PkStream *device)
{
    if (!resource || !resource->valid() || resource->resourceId() < 0) return false;

    const PkString resourceUrl = resource->resourceType().first + "/" + resource->filename();
    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(resource->storageLocation()));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, false);
    return storage->exportResource(resourceUrl, device);
}

bool KisResourceLocator::addResource(const PkString &resourceType, const KoResourceSP resource, const PkString &storageLocation)
{
    if (!resource || !resource->valid()) return false;

    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(storageLocation));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, false);

    //If we have gotten this far and the resource still doesn't have a filename to save to, we should generate one.
    if (resource->filename().isEmpty()) {
        resource->setFilename(replaceSpaces(resource->name()) + resource->defaultFileExtension());
    }

    if (resource->version() != 0) { // Can happen with cloned resources
        resource->setVersion(0);
    }

    // Save the resource to the storage storage
    if (!storage->addResource(resource)) {
        qWarning() << "Could not add resource" << resource->filename() << "to the storage" << storageLocation;
        return false;
    }

    resource->setStorageLocation(storageLocation);
    resource->setMD5Sum(storage->resourceMd5(resourceType + "/" + resource->filename()));
    resource->setDirty(false);
    loadRequiredResources(resource);

    d->resourceCache[std::pair<PkString, PkString>(storageLocation, resourceType + "/" + resource->filename())] = resource;

    /// And to the database.
    ///
    /// The metadata will be set by KisResourceCacheDb, which is
    /// not very consistent with KisResourceLocator::updateResource(),
    /// but works :)
    const bool result = KisResourceCacheDb::addResource(storage,
                                                        storage->timeStampForResource(resourceType, resource->filename()),
                                                        resource,
                                                        resourceType);
    return result;
}

bool KisResourceLocator::updateResource(const PkString &resourceType, const KoResourceSP resource)
{
    PkString storageLocation = makeStorageLocationAbsolute(resource->storageLocation());

    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(storageLocation));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, false);

    if (resource->resourceId() < 0) {
        return addResource(resourceType, resource);
    }

    if (!storage->supportsVersioning()) return false;

    // remove older version
    KisResourceThumbnailCache::instance()->remove(storageLocation, resourceType, resource->filename());

    resource->updateThumbnail();
    resource->setVersion(resource->version() + 1);
    resource->setActive(true);

    if (!storage->saveAsNewVersion(resource)) {
        qWarning() << "Failed to save the new version of " << resource->name() << "to storage" << storageLocation;
        return false;
    }

    resource->setMD5Sum(storage->resourceMd5(resourceType + "/" + resource->filename()));
    resource->setDirty(false);
    loadRequiredResources(resource);

    // The version needs already to have been incremented
    if (!KisResourceCacheDb::addResourceVersion(resource->resourceId(), PkDateTime::currentDateTime(), storage, resource)) {
        qWarning() << "Failed to add a new version of the resource to the database" << resource->name();
        return false;
    }

    if (!setMetaDataForResource(resource->resourceId(), resource->metadata())) {
        qWarning() << "Failed to update resource metadata" << resource;
        return false;
    }

    // Update the resource in the cache
    std::pair<PkString, PkString> key = std::pair<PkString, PkString> (storageLocation, resourceType + "/" + resource->filename());
    d->resourceCache[key] = resource;
    KisResourceThumbnailCache::instance()->insert(key, resource->thumbnail());

    return true;
}

bool KisResourceLocator::reloadResource(const PkString &resourceType, const KoResourceSP resource)
{
    // This resource isn't in the database yet, so we cannot reload it
    if (resource->resourceId() < 0) return false;

    PkString storageLocation = makeStorageLocationAbsolute(resource->storageLocation());
    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(storageLocation));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, false);

    if (!storage->loadVersionedResource(resource)) {
        qWarning() << "Failed to reload the resource" << resource->name() << "from storage" << storageLocation;
        return false;
    }

    resource->setMD5Sum(storage->resourceMd5(resourceType + "/" + resource->filename()));
    resource->setDirty(false);
    loadRequiredResources(resource);

    // We haven't changed the version of the resource, so the cache must be still valid
    std::pair<PkString, PkString> key = std::pair<PkString, PkString> (storageLocation, resourceType + "/" + resource->filename());
    KIS_ASSERT(d->resourceCache[key] == resource);

    return true;
}

PkMap<PkString, PkVariant> KisResourceLocator::metaDataForResource(int id) const
{
    return KisResourceCacheDb::metaDataForId(id, "resources");
}

KisResourceCacheDb::MetaDataReadResult
KisResourceLocator::metaDataReadResultForResource(int id) const
{
    return KisResourceCacheDb::metaDataReadResultForId(id, "resources");
}

bool KisResourceLocator::setMetaDataForResource(int id, PkMap<PkString, PkVariant> map) const
{
    return KisResourceCacheDb::updateMetaDataForId(map, id, "resources");
}

PkMap<PkString, PkVariant> KisResourceLocator::metaDataForStorage(const PkString &storageLocation) const
{
    PkMap<PkString, PkVariant> metadata;

    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(storageLocation));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, metadata);

    for (const PkString &key : storage->metaDataKeys()) {
        metadata[key] = storage->metaData(key);
    }
    return metadata;
}

void KisResourceLocator::setMetaDataForStorage(const PkString &storageLocation, PkMap<PkString, PkVariant> map) const
{
    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(storageLocation));
    KIS_SAFE_ASSERT_RECOVER_RETURN(storage);

    for (const PkString &key : map.keys()) {
        storage->setMetaData(key, map[key]);
    }
}

void KisResourceLocator::purge(const PkString &storageLocation, const PkVector<int> &removedTagIds)
{
    for (const auto &key : d->resourceCache.keys()) {
        if (key.first == storageLocation) {
            d->resourceCache.remove(key);
            KisResourceThumbnailCache::instance()->remove(key);
        }
    }

    for (auto it = d->tagCache.begin(); it != d->tagCache.end();) {
        if (removedTagIds.contains(it.value()->id())) {
            it = d->tagCache.erase(it);
        } else {
            ++it;
        }
    }
}

bool KisResourceLocator::addStorage(const PkString &storageLocation, KisResourceStorageSP storage)
{
    if (d->storages.contains(storageLocation)) {
        if (!removeStorage(storageLocation)) {
            qWarning() << "could not remove" << storageLocation;
            return false;
        }
    }

    PkVector<std::pair<PkString, int>> addedResources;
    for (const PkString &type : KisResourceLoaderRegistry::instance()->resourceTypes()) {
        int numAddedResources = 0;

        PkSharedPointer<KisResourceStorage::ResourceIterator> it = storage->resources(type);
        while (it->hasNext()) {
            it->next();
            numAddedResources++;
        }

        if (numAddedResources > 0) {
            addedResources << std::make_pair(type, numAddedResources);
        }
    }

    for (const auto &typedResources : addedResources) {
        beginExternalResourceImport(typedResources.first, typedResources.second);
    }

    d->storages[storageLocation] = storage;
    if (!KisResourceCacheDb::addStorage(storage, false)) {
        d->errorMessages.append(PkString("Could not add %1 to the database").arg(storage->location()));
        qWarning() << d->errorMessages;
        return false;
    }

    if (!KisResourceCacheDb::addStorageTags(storage)) {
        d->errorMessages.append(PkString("Could not add tags for storage %1 to the cache database").arg(storage->location()));
        qWarning() << d->errorMessages;
        return false;
    }

    for (const auto &typedResources : addedResources) {
        endExternalResourceImport(typedResources.first);
    }

    storageAdded(makeStorageLocationRelative(storage->location()));
    return true;
}

bool KisResourceLocator::removeStorage(const PkString &storageLocation)
{
    // Cloned documents have a document storage, but that isn't in the locator.
    if (!d->storages.contains(storageLocation)) {
        return true;
    }

    PkVector<std::pair<PkString, PkVector<int>>> removedResources;

    for (const PkString &type : KisResourceLoaderRegistry::instance()->resourceTypes()) {
        const PkVector<int> resources = KisResourceCacheDb::resourcesForStorage(type, storageLocation);
        if (!resources.isEmpty()) {
            removedResources << std::make_pair(type, resources);
        }
    }

    PkVector<std::pair<PkString, PkVector<int>>> removedTags;
    PkVector<int> allRemovedTagIds;
    for (const PkString &type : KisResourceLoaderRegistry::instance()->resourceTypes()) {
        auto [uniqueTags, sharedTags] = KisResourceCacheDb::tagsForStorage(type, storageLocation);
        removedTags << std::make_pair(type, uniqueTags);
        allRemovedTagIds.append(uniqueTags);
    }

    for (const auto &typedResources : removedResources) {
        beginExternalResourceRemove(typedResources.first, typedResources.second);
    }

    // TODO: add model notification about removed tags
    (void)removedTags;

    purge(storageLocation, allRemovedTagIds);

    KisResourceStorageSP storage = d->storages.take(storageLocation);

    if (!KisResourceCacheDb::deleteStorage(storage)) {
        d->storages.insert(storageLocation, storage);
        for (const auto &typedResources : removedResources) {
            endExternalResourceRemove(typedResources.first);
        }
        d->errorMessages.append(PkString("Could not remove storage %1 from the database").arg(storage->location()));
        qWarning() << d->errorMessages;
        return false;
    }

    for (const auto &typedResources : removedResources) {
        endExternalResourceRemove(typedResources.first);
    }

    storageRemoved(makeStorageLocationRelative(storage->location()));

    return true;
}

bool KisResourceLocator::hasStorage(const PkString &document)
{
    return d->storages.contains(document);
}

void KisResourceLocator::saveTags()
{
    PkSqlQuery query;

    if (!query.prepare("SELECT tags.url \n"
                       ",      resource_types.name \n"
                       "FROM   tags\n"
                       ",      resource_types\n"
                       "WHERE  tags.resource_type_id = resource_types.id\n"))
    {
        qWarning() << "Could not prepare save tags query" << query.lastError();
        return;
    }

    if (!query.exec()) {
        qWarning() << "Could not execute save tags query" << query.lastError();
        return;
    }

    // this needs to use ResourcePaths because it is sometimes called during initialization
    // (when the database versions don't match up and tags need to be saved)
    PkString resourceLocation = KoResourcePaths::getAppDataLocation() + "/";

    while (query.next()) {
        // Save tag...
        KisTagSP tag = tagForUrlNoCache(query.value("tags.url").toString(),
                                 query.value("resource_types.name").toString());

        if (!tag || !tag->valid()) {
            continue;
        }


        PkString filename = tag->filename();
        if (filename.isEmpty() || suffix(filename).isEmpty()) {
            filename = tag->url() + ".tag";
        }

        if (lowerAscii(suffix(filename).PkToUtf8()) != "tag") {
            // it's either .abr file, or maybe a .bundle
            // or something else, but not a tag file
            debugResource << "Skipping saving tag " << tag->name(false) << filename << tag->resourceType();
            continue;
        }

        filename = removeBasePath(filename, resourceLocation);
        const fs::path outputPath = toPath(resourceLocation + "/" + tag->resourceType() + "/" + filename);

        PkMemoryStream buf;
        buf.open(static_cast<PkStream::OpenMode>(PkStream::WriteOnly | PkStream::Truncate));

        if (!tag->save(buf)) {
            qWarning() << "Could not save tag to" << fromPath(outputPath);
            continue;
        }

        if (!writeFile(outputPath, buf.data(), static_cast<std::size_t>(buf.size()))) {
            qWarning() << "Could not open tag file for writing" << fromPath(outputPath);
        }
    }
}

void KisResourceLocator::purgeTag(const PkString tagUrl, const PkString resourceType)
{
    d->tagCache.remove(std::pair<PkString, PkString>(resourceType, tagUrl));
}

PkString KisResourceLocator::filePathForResource(KoResourceSP resource)
{
    KisResourceStorageSP storage = d->safeGetStorage(makeStorageLocationAbsolute(resource->storageLocation()));
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(storage, PkString());

    const PkString resourceUrl = resource->resourceType().first + "/" + resource->filename();

    return storage->resourceFilePath(resourceUrl);
}

void KisResourceLocator::updateFontStorage()
{
    if (!KisResourceCacheDb::synchronizeStorage(fontStorage())) {
        qWarning() << "Could not synchronize updated font registry with the database";
    } else {
        storageResynchronized(fontStorage()->location(), false);
    }
}

KisResourceLocator::LocatorError KisResourceLocator::firstTimeInstallation(InitializationStatus initializationStatus, const PkString &installationResourcesLocation)
{
    progressMessage(PkString("Krita is running for the first time. Initialization will take some time."));
    (void)initializationStatus;
    PkResourceStorageDesktop desktop;

    for (const PkString &folder : KisResourceLoaderRegistry::instance()->resourceTypes()) {
        const PkString destination = d->resourceLocation + "/" + folder + "/";
        if (!desktop.exists(destination) && !desktop.mkpath(destination)) {
            d->errorMessages.append(PkString("3. Could not create the resource location at %1.").arg(destination));
            return LocatorError::CannotCreateLocation;
        }
    }

    for (const PkString &folder : KisResourceLoaderRegistry::instance()->resourceTypes()) {
        const PkString sourceDirectory = installationResourcesLocation + "/" + folder + "/";
        auto entries = desktop.listEntries(sourceDirectory, {}, PkResourceStorage::EntryKind::Files, false);
        while (entries->hasNext()) {
            entries->next();
            const fs::path source = toPath(entries->url());
            const fs::path destination = toPath(d->resourceLocation + "/" + folder) / source.filename();
            std::error_code ec;
            if (!fs::exists(destination, ec) && !fs::copy_file(source, destination, ec)) {
                d->errorMessages.append(PkString("Could not copy resource %1 to %2")
                                            .arg(fromPath(source), fromPath(destination)));
            }
        }
    }

    const std::vector<PkString> filters{
        PkString("*.bundle"), PkString("*.abr"), PkString("*.asl")};
    auto bundles = desktop.listEntries(installationResourcesLocation,
                                       filters,
                                       PkResourceStorage::EntryKind::Files,
                                       true);
    while (bundles->hasNext()) {
        bundles->next();
        const fs::path source = toPath(bundles->url());
        const fs::path destination = toPath(d->resourceLocation) / source.filename();
        progressMessage(PkString("Installing the resources from bundle %1.").arg(fromPath(source)));
        std::error_code ec;
        if (!fs::copy_file(source, destination, fs::copy_options::skip_existing, ec) && ec) {
            d->errorMessages.append(PkString("Could not copy resource %1 to %2")
                                        .arg(fromPath(source), d->resourceLocation));
        }
    }

    const std::string version = KritaVersionWrapper::versionString().PkToUtf8();
    const fs::path versionPath = toPath(d->resourceLocation) / "KRITA_RESOURCE_VERSION";
    if (!writeFile(versionPath, version.data(), version.size())) {
        qWarning() << "Could not open" << fromPath(versionPath) << "for writing";
    }

    return LocatorError::Ok;
}

void KisResourceLocator::findStorages()
{
    d->storages.clear();
    d->resourceCache.clear();

    // Add the folder
    KisResourceStorageSP storage = PkSharedPointer<KisResourceStorage>::create(d->resourceLocation);
    KIS_ASSERT(storage->location() == d->resourceLocation);
    d->storages[d->resourceLocation] = storage;

    // Add the memory storage
    d->storages["memory"] = PkSharedPointer<KisResourceStorage>::create("memory");
    d->storages["memory"]->setMetaData(KisResourceStorage::s_meta_name, PkString("Temporary Resources"));

    // Add font storage
    auto fontStorage = PkSharedPointer<KisResourceStorage>::create("fontregistry");
    if (fontStorage && fontStorage->valid()) {
        d->storages["fontregistry"] = fontStorage;
        d->storages["fontregistry"]->setMetaData(KisResourceStorage::s_meta_name, PkString("Font Storage"));
    }

    // And add bundles and adobe libraries
    PkResourceStorageDesktop desktop;
    const std::vector<PkString> filters{
        PkString("*.bundle"), PkString("*.abr"), PkString("*.asl")};
    auto entries = desktop.listEntries(d->resourceLocation,
                                       filters,
                                       PkResourceStorage::EntryKind::Files,
                                       true);
    while (entries->hasNext()) {
        entries->next();
        KisResourceStorageSP storage = PkSharedPointer<KisResourceStorage>::create(entries->url());
        if (!storage->valid()) {
            // we still add the storage to the list and try to read whatever possible
            qWarning() << "KisResourceLocator::findStorages: the storage is invalid" << storage->location();
        }
        d->storages[storage->location()] = storage;
    }

    // Add any missing storage types to the resource cache database.
    for (const KisResourceStorage::StorageType &type : KisStoragePluginRegistry::instance()->storageTypes()) {
        KisResourceCacheDb::registerStorageType(type);
    }
}

PkList<KisResourceStorageSP> KisResourceLocator::storages() const
{
    return d->storages.values();
}

KisResourceStorageSP KisResourceLocator::storageByLocation(const PkString &location) const
{
    KisResourceStorageSP storage = d->safeGetStorage(location);
    if (!storage || !storage->valid()) {
        qWarning() << "Could not retrieve the" << location << "storage object or the object is not valid";
        return 0;
    }

    return storage;
}

KisResourceStorageSP KisResourceLocator::folderStorage() const
{
    return storageByLocation(d->resourceLocation);
}

KisResourceStorageSP KisResourceLocator::memoryStorage() const
{
    return storageByLocation("memory");
}

KisResourceStorageSP KisResourceLocator::fontStorage() const
{
    return storageByLocation("fontregistry");
}

KisResourceLocator::ResourceStorage KisResourceLocator::getResourceStorage(int resourceId) const
{
    ResourceStorage rs;

    PkSqlQuery q;
    bool r = q.prepare("SELECT storages.location\n"
                       ",      resource_types.name as resource_type\n"
                       ",      resources.filename\n"
                       "FROM   resources\n"
                       ",      storages\n"
                       ",      resource_types\n"
                       "WHERE  resources.id = :resource_id\n"
                       "AND    resources.storage_id = storages.id\n"
                       "AND    resource_types.id = resources.resource_type_id");
    if (!r) {
        qWarning() << "KisResourceLocator::removeResource: could not prepare query." << q.lastError();
        return rs;
    }


    q.bindValue(":resource_id", resourceId);

    r = q.exec();
    if (!r) {
        qWarning() << "KisResourceLocator::removeResource: could not execute query." << q.lastError();
        return rs;
    }

    q.first();

    PkString storageLocation = q.value("location").toString();
    PkString resourceType= q.value("resource_type").toString();
    PkString resourceFilename = q.value("filename").toString();

    rs.storageLocation = makeStorageLocationAbsolute(storageLocation);
    rs.resourceType = resourceType;
    rs.resourceFileName = resourceFilename;

    return rs;
}

PkString KisResourceLocator::makeStorageLocationAbsolute(PkString storageLocation) const
{
//    debugResource << "makeStorageLocationAbsolute" << storageLocation;

    if (storageLocation.isEmpty()) {
        return resourceLocationBase();
    }

    if (toPath(storageLocation).is_relative() &&
        (endsWithAsciiCaseInsensitive(storageLocation, ".bundle") ||
         endsWithAsciiCaseInsensitive(storageLocation, ".asl") ||
         endsWithAsciiCaseInsensitive(storageLocation, ".abr"))) {
        if (endsWithAsciiCaseInsensitive(resourceLocationBase(), "/") ||
            endsWithAsciiCaseInsensitive(resourceLocationBase(), "\\")) {
            storageLocation = resourceLocationBase() + storageLocation;
        }
        else {
            storageLocation = resourceLocationBase() + "/" + storageLocation;
        }
    }

//    debugResource  << "\t" << storageLocation;
    return storageLocation;
}

bool KisResourceLocator::synchronizeDb()
{
    progressMessage(PkString("Synchronizing the resources."));

    d->errorMessages.clear();

    // Add resource types that have been added since first-time installation.
    for (auto loader : KisResourceLoaderRegistry::instance()->values()) {
        KisResourceCacheDb::registerResourceType(loader->resourceType());
    }


    findStorages();
    for (const KisResourceStorageSP &storage : d->storages) {
        if (!KisResourceCacheDb::synchronizeStorage(storage)) {
            d->errorMessages.append(PkString("Could not synchronize %1 with the database").arg(storage->location()));
        } else {
            storageResynchronized(storage->location(), true);
        }
    }

    for (const KisResourceStorageSP &storage : d->storages) {
        if (!KisResourceCacheDb::addStorageTags(storage)) {
            d->errorMessages.append(PkString("Could not synchronize %1 with the database").arg(storage->location()));
        }
    }

    /**
     * In the current layout of the database we cannot set FOREIGN KEY
     * for the metadata table (since it links to both, resources and storages),
     * hence we should manually track the orphaned data.
     *
     * Theoretically, these should be none, if our code is correct, but who 
     * knows anything about our code...
     */
    if (!KisResourceCacheDb::removeOrphanedMetaData()) {
        d->errorMessages.append(PkString("Could not remove orphaned metadata"));
        return false;
    }

    // Remove database rows whose storage no longer exists. Query the cache
    // directly; the model layer is a later task and is not part of this core.
    PkList<PkString> storagesToRemove;
    PkSqlQuery storageQuery;
    if (!storageQuery.exec(PkString("SELECT location FROM storages ORDER BY id"))) {
        d->errorMessages.append(PkString("Could not enumerate storages in the database"));
        return false;
    }
    while (storageQuery.next()) {
        storagesToRemove.append(storageQuery.value(0).toString());
    }

    for (int i = 0; i < storagesToRemove.size(); i++) {
        PkString location = storagesToRemove[i];
        if (!d->storages.contains(this->makeStorageLocationAbsolute(location))) {
            if (!KisResourceCacheDb::deleteStorage(location)) {
                d->errorMessages.append(PkString("Could not remove storage %1 from the database")
                                            .arg(this->makeStorageLocationAbsolute(location)));
                qWarning() << d->errorMessages;
                return false;
            }
            storageRemoved(this->makeStorageLocationAbsolute(location));
        }
    }


    d->errorMessages.append(KisResourceLoaderRegistry::instance()->executeAllFixups());

    d->resourceCache.clear();
    if (d->errorMessages.isEmpty()) {
        storagesBulkSynchronizationFinished();
    }
    return d->errorMessages.isEmpty();
}


PkString KisResourceLocator::makeStorageLocationRelative(PkString location) const
{
//    debugResource << "makeStorageLocationRelative" << location << "locationbase" << resourceLocationBase();
    return removeBasePath(location, resourceLocationBase());
}
