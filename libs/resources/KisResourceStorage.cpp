/*
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisResourceStorage.h"

#include <cmath>
#include <boost/optional.hpp>
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <regex>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <kis_assert.h>


#include "KisFolderStorage.h"
#include "KisBundleStorage.h"
#include "KisMemoryStorage.h"
#include "PkResourceStorageDesktop.h"
#include "KisResourceThumbnailCodec.h"
#include "ResourceDebug.h"

#include <PkFileStream.h>
#include <PkZipArchive.h>


namespace {
namespace fs = std::filesystem;

std::string lowerAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text;
}

bool endsWithAsciiCaseInsensitive(const PkString &value, const char *suffix)
{
    const std::string text = lowerAscii(value.PkToUtf8());
    const std::string tail = lowerAscii(suffix);
    return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

PkString fileName(const PkString &path)
{
    return PkString(fs::path(path.PkToUtf8()).filename().string().c_str());
}

PkDateTime lastModified(const PkString &path, const PkResourceStorageDesktop &storage)
{
    const int64_t timestamp = storage.lastModified(path);
    return timestamp ? PkDateTime::fromMSecsSinceEpoch(timestamp) : PkDateTime();
}

bool looksLikeUuid(const std::string &text)
{
    static const std::regex uuid("^\\{?[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\\}?$");
    return std::regex_match(text, uuid);
}

bool pathAccess(const PkString &path, int mode)
{
#ifdef _WIN32
    return ::_access(path.PkToUtf8().c_str(), mode) == 0;
#else
    return ::access(path.PkToUtf8().c_str(), mode) == 0;
#endif
}

KisResourceStorage::StorageType autoDetectStorageType(const PkString &location) {
    PkResourceStorageDesktop storage;
    const fs::path native(location.PkToUtf8());
    if (fs::is_directory(native)) {
        return KisResourceStorage::StorageType::Folder;
    } else if (endsWithAsciiCaseInsensitive(location, ".bundle")) {
        return KisResourceStorage::StorageType::Bundle;
    } else if (endsWithAsciiCaseInsensitive(location, ".abr")) {
        return KisResourceStorage::StorageType::AdobeBrushLibrary;
    } else if (endsWithAsciiCaseInsensitive(location, ".asl")) {
        return KisResourceStorage::StorageType::AdobeStyleLibrary;
    } else if (location == "fontregistry") {
        return KisResourceStorage::StorageType::FontStorage;
    } else if (location == "memory" || looksLikeUuid(location.PkToUtf8()) || (!location.isEmpty() && !storage.exists(location))) {
        return KisResourceStorage::StorageType::Memory;
    }

    return KisResourceStorage::StorageType::Unknown;
}
} // namespace

const PkString KisResourceStorage::s_xmlns_meta("urn:oasis:names:tc:opendocument:xmlns:meta:1.0");
const PkString KisResourceStorage::s_xmlns_dc("http://purl.org/dc/elements/1.1");

const PkString KisResourceStorage::s_meta_generator("meta:generator");
const PkString KisResourceStorage::s_meta_author("dc:author");
const PkString KisResourceStorage::s_meta_title("dc:title");
const PkString KisResourceStorage::s_meta_description("dc:description");
const PkString KisResourceStorage::s_meta_initial_creator("meta:initial-creator");
const PkString KisResourceStorage::s_meta_creator("dc:creator");
const PkString KisResourceStorage::s_meta_creation_date("meta:creation-date");
const PkString KisResourceStorage::s_meta_dc_date("meta:dc-date");
const PkString KisResourceStorage::s_meta_user_defined("meta:meta-userdefined");
const PkString KisResourceStorage::s_meta_name("meta:name");
const PkString KisResourceStorage::s_meta_value("meta:value");
const PkString KisResourceStorage::s_meta_version("meta:bundle-version");
const PkString KisResourceStorage::s_meta_email("meta:email");
const PkString KisResourceStorage::s_meta_license("meta:license");
const PkString KisResourceStorage::s_meta_website("meta:website");

KisStoragePluginRegistry::KisStoragePluginRegistry()
{
    m_storageFactoryMap[KisResourceStorage::StorageType::Folder] = new KisStoragePluginFactory<KisFolderStorage>();
    m_storageFactoryMap[KisResourceStorage::StorageType::Memory] = new KisStoragePluginFactory<KisMemoryStorage>();
    m_storageFactoryMap[KisResourceStorage::StorageType::Bundle] = new KisStoragePluginFactory<KisBundleStorage>();
}

KisStoragePluginRegistry::~KisStoragePluginRegistry()
{
    for (KisStoragePluginFactoryBase *factory : m_storageFactoryMap.values()) delete factory;
}

void KisStoragePluginRegistry::addStoragePluginFactory(KisResourceStorage::StorageType storageType, KisStoragePluginFactoryBase *factory)
{
    m_storageFactoryMap[storageType] = factory;
}

PkList<KisResourceStorage::StorageType> KisStoragePluginRegistry::storageTypes() const
{
    return m_storageFactoryMap.keys();
}

KisStoragePluginRegistry *KisStoragePluginRegistry::instance()
{
    static KisStoragePluginRegistry registry;
    return &registry;
}

class KisResourceStorage::Private {
public:
    PkString name;
    PkString location;
    bool valid {false};
    KisResourceStorage::StorageType storageType {KisResourceStorage::StorageType::Unknown};
    PkSharedPointer<KisStoragePlugin> storagePlugin;
    int storageId {-1};
};

KisResourceStorage::KisResourceStorage(const PkString &location, KisResourceStorage::StorageType storageType)
    : d(new Private())
{
    d->location = location;

    auto createMemoryStorage = [this] (const PkString &location, bool isValid) {
        d->name = location;
        d->storageType = StorageType::Memory;
        d->storagePlugin.reset(KisStoragePluginRegistry::instance()->m_storageFactoryMap[StorageType::Memory]->create(location));
        d->valid = isValid;
    };

    switch (storageType) {
        case StorageType::Folder: {
            PkResourceStorageDesktop storage;
            if (fs::is_directory(fs::path(d->location.PkToUtf8()))) {
                d->name = fileName(d->location);
                d->storageType = StorageType::Folder;
                d->storagePlugin.reset(KisStoragePluginRegistry::instance()->m_storageFactoryMap[StorageType::Folder]->create(location));
                d->valid = pathAccess(d->location, 2);
            } else {
                createMemoryStorage(location, false);
            }
            break;
        }
        case StorageType::Bundle: {
            d->name = fileName(d->location);
            d->storageType = StorageType::Bundle;
            d->storagePlugin.reset(KisStoragePluginRegistry::instance()->m_storageFactoryMap[StorageType::Bundle]->create(location));
            // XXX: should we also check whether there's a valid metadata entry? Or is this enough?
            PkZipArchive archive(PkZipArchive::Read);
            d->valid = archive.openFile(d->location);
            if (d->valid) archive.close();
            break;
        }
        case StorageType::AdobeBrushLibrary: {
            d->name = fileName(d->location);
            d->storageType = StorageType::AdobeBrushLibrary;
            d->storagePlugin.reset(KisStoragePluginRegistry::instance()->m_storageFactoryMap[StorageType::AdobeBrushLibrary]->create(location));
            d->valid = pathAccess(d->location, 4);
            break;
        }
        case StorageType::AdobeStyleLibrary: {
            d->name = fileName(d->location);
            d->storageType = StorageType::AdobeStyleLibrary;
            d->storagePlugin.reset(KisStoragePluginRegistry::instance()->m_storageFactoryMap[StorageType::AdobeStyleLibrary]->create(location));
            d->valid = d->storagePlugin->isValid();
            break;
        }
        case StorageType::Unknown:
            createMemoryStorage(location, false);
            break;
        case StorageType::Memory:
            createMemoryStorage(location, true);
            break;
        case StorageType::FontStorage: {
            auto factory = KisStoragePluginRegistry::instance()->m_storageFactoryMap[StorageType::FontStorage];
            if (factory) {
                d->name = location;
                d->storageType = StorageType::FontStorage;
                d->storagePlugin.reset(factory->create(location));
                d->valid = true;
            } else {
                createMemoryStorage(location, false);
            }
            break;
        }
    }
}

KisResourceStorage::KisResourceStorage(const PkString &location)
    : KisResourceStorage(location, autoDetectStorageType(location))
{
}

KisResourceStorage::~KisResourceStorage()
{
}

KisResourceStorage::KisResourceStorage(const KisResourceStorage &rhs)
    : d(new Private)
{
    *this = rhs;
}

KisResourceStorage &KisResourceStorage::operator=(const KisResourceStorage &rhs)
{
    if (this != &rhs) {
        d->name = rhs.d->name;
        d->location = rhs.d->location;
        d->storageType = rhs.d->storageType;
        if (d->storageType == StorageType::Memory) {
            const PkSharedPointer<KisMemoryStorage> memoryStorage = rhs.d->storagePlugin.dynamicCast<KisMemoryStorage>();
            KIS_ASSERT(memoryStorage);
            d->storagePlugin = PkSharedPointer<KisMemoryStorage>(new KisMemoryStorage(*memoryStorage));
        }
        else {
            d->storagePlugin = rhs.d->storagePlugin;
        }
        d->valid = false;
    }
    return *this;
}

KisResourceStorageSP KisResourceStorage::clone() const
{
    return KisResourceStorageSP(new KisResourceStorage(*this));
}

PkString KisResourceStorage::name() const
{
    return d->name;
}

PkString KisResourceStorage::location() const
{
    return d->location;
}

KisResourceStorage::StorageType KisResourceStorage::type() const
{
    return d->storageType;
}

PkImage KisResourceStorage::thumbnail() const
{
    return d->storagePlugin->thumbnail();
}

PkDateTime KisResourceStorage::timestamp() const
{
    return d->storagePlugin->timestamp();
}

PkDateTime KisResourceStorage::timeStampForResource(const PkString &resourceType, const PkString &filename) const
{
    PkResourceStorageDesktop storage;
    if (endsWithAsciiCaseInsensitive(d->location, ".bundle")) {
        const PkString modified = d->location + "_modified/" + resourceType + "/" + filename;
        if (storage.exists(modified)) {
            return lastModified(modified, storage);
        }
    } else {
        const PkString resourcePath = d->location + "/" + resourceType + "/" + filename;
        if (storage.exists(resourcePath)) return lastModified(resourcePath, storage);
    }
    return this->timestamp();
}

KisResourceStorage::ResourceItem KisResourceStorage::resourceItem(const PkString &url)
{
    return d->storagePlugin->resourceItem(url);
}

KoResourceSP KisResourceStorage::resource(const PkString &url)
{
    return d->storagePlugin->resource(url);
}

PkString KisResourceStorage::resourceMd5(const PkString &url)
{
    return d->storagePlugin->resourceMd5(url);
}

PkString KisResourceStorage::resourceFilePath(const PkString &url)
{
    return d->storagePlugin->resourceFilePath(url);
}

PkSharedPointer<KisResourceStorage::ResourceIterator> KisResourceStorage::resources(const PkString &resourceType) const
{
    return d->storagePlugin->resources(resourceType);
}

PkSharedPointer<KisResourceStorage::TagIterator> KisResourceStorage::tags(const PkString &resourceType) const
{
    return d->storagePlugin->tags(resourceType);
}

bool KisResourceStorage::saveAsNewVersion(KoResourceSP resource)
{
    if (!resource) return false;

    return d->storagePlugin->saveAsNewVersion(resource->resourceType().first, resource);
}

bool KisResourceStorage::addResource(KoResourceSP resource)
{
    if (!resource) return false;

    return d->storagePlugin->addResource(resource->resourceType().first, resource);
}

bool KisResourceStorage::importResource(const PkString &url, PkStream *device)
{
    return d->storagePlugin->importResource(url, device);
}

bool KisResourceStorage::exportResource(const PkString &url, PkStream *device)
{
    return d->storagePlugin->exportResource(url, device);
}

bool KisResourceStorage::supportsVersioning() const
{
    return d->storagePlugin->supportsVersioning();
}

bool KisResourceStorage::loadVersionedResource(KoResourceSP resource)
{
    return d->storagePlugin->loadVersionedResource(resource);
}

void KisResourceStorage::setMetaData(const PkString &key, const PkVariant &value)
{
    d->storagePlugin->setMetaData(key, value);
}

bool KisResourceStorage::valid() const
{
    return d->valid;
}

PkStringList KisResourceStorage::metaDataKeys() const
{
    return d->storagePlugin->metaDataKeys();
}

PkVariant KisResourceStorage::metaData(const PkString &key) const
{
    return d->storagePlugin->metaData(key);
}

void KisResourceStorage::setStorageId(int storageId)
{
    d->storageId = storageId;
}

int KisResourceStorage::storageId()
{
    return d->storageId;
}

KisStoragePlugin* KisResourceStorage::testingGetStoragePlugin()
{
    return d->storagePlugin.data();
}

struct VersionedFileParts
{
    PkString basename;
    int version = 0;
    PkString suffix;
};

boost::optional<VersionedFileParts> guessFilenameParts(const PkString &filename)
{
    static const std::regex expression("^(.*)\\.([0-9][0-9]*)\\.(.+)$");
    std::smatch match;
    const std::string text = filename.PkToUtf8();
    if (std::regex_match(text, match, expression)) {
        const std::string versionText = match[2].str();
        int version = 0;
        const auto parsed = std::from_chars(versionText.data(),
                                            versionText.data() + versionText.size(),
                                            version);
        if (parsed.ec != std::errc() || parsed.ptr != versionText.data() + versionText.size()) {
            version = 0;
        }
        return VersionedFileParts({PkString(match[1].str().c_str()), version,
                                   PkString(match[3].str().c_str())});
    }

    return boost::none;
}

VersionedFileParts guessFileNamePartsLazy(const PkString &filename, int minVersion)
{
    boost::optional<VersionedFileParts> guess = guessFilenameParts(filename);
    if (guess) {
        guess->version = std::max(guess->version, minVersion);
    } else {
        const fs::path info(filename.PkToUtf8());
        guess = VersionedFileParts();
        const std::string fullName = info.filename().string();
        const std::size_t firstDot = fullName.find('.');
        guess->basename = PkString((firstDot == std::string::npos ? fullName : fullName.substr(0, firstDot)).c_str());
        guess->version = minVersion;
        guess->suffix = PkString((firstDot == std::string::npos ? std::string() : fullName.substr(firstDot + 1)).c_str());
    }

    return *guess;
}

PkString KisStorageVersioningHelper::chooseUniqueName(KoResourceSP resource,
                                                     int minVersion,
                                                     std::function<bool(PkString)> checkExists)
{
    int version = std::max(resource->version(), minVersion);

    VersionedFileParts parts = guessFileNamePartsLazy(resource->filename(), version);
    version = parts.version;

    PkString newFilename;

    while (1) {
        int numPlaceholders = 4;

        if (version > 9999) {
            numPlaceholders = static_cast<int>(std::floor(std::log10(version))) + 1;
        }

        char versionBuffer[64];
        std::snprintf(versionBuffer, sizeof(versionBuffer), "%0*d", numPlaceholders, version);
        const PkString versionString(versionBuffer);

        // XXX: Temporary, until I've fixed the tests
        if (versionString == "0000") {
            newFilename = resource->filename();
        }
        else {
            newFilename = parts.basename +
                    "."
                    + versionString
                    + "."
                    + parts.suffix;
        }
        if (checkExists(newFilename)) {
            version++;
            if (version == std::numeric_limits<int>::max()) {
                return PkString();
            }
            continue;
        }

        break;
    }

    return newFilename;
}

void KisStorageVersioningHelper::detectFileVersions(PkVector<VersionedResourceEntry> &allFiles)
{
    for (auto it = allFiles.begin(); it != allFiles.end(); ++it) {
        VersionedFileParts parts = guessFileNamePartsLazy(it->filename, -1);
        it->guessedKey = parts.basename + parts.suffix;
        it->guessedVersion = parts.version;
    }

    std::sort(allFiles.begin(), allFiles.end(), VersionedResourceEntry::KeyVersionLess());

    boost::optional<PkString> lastResourceKey;
    int availableVersion = 0;
    for (auto it = allFiles.begin(); it != allFiles.end(); ++it) {
        if (!lastResourceKey || *lastResourceKey != it->guessedKey) {
            availableVersion = 0;
            lastResourceKey = it->guessedKey;
        }

        if (it->guessedVersion < availableVersion) {
            it->guessedVersion = availableVersion;
        }

        availableVersion = it->guessedVersion + 1;
    }
}

bool KisStorageVersioningHelper::addVersionedResource(const PkString &saveLocation,
                                                      KoResourceSP resource,
                                                      int minVersion)
{
    int version = std::max(resource->version(), minVersion);

    VersionedFileParts parts = guessFileNamePartsLazy(resource->filename(), version);
    version = parts.version;

    PkString newFilename =
        chooseUniqueName(resource, minVersion,
                         [saveLocation] (const PkString &filename) {
                             return PkResourceStorageDesktop().exists(
                                 PkResourceStorage::joinPath(saveLocation, filename));
                         });

    if (newFilename.isEmpty()) return false;

    const PkString path = PkResourceStorage::joinPath(saveLocation, newFilename);
    PkResourceStorageDesktop storage;
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!storage.exists(path), false);

    PkFileStream file(path);
    if (!file.open(PkStream::WriteOnly | PkStream::Truncate)) {
        qCWarning(RESOURCE_LOG) << "Could not open resource file for writing" << newFilename;
        return false;
    }

    if (!resource->saveToDevice(&file)) {
        qCWarning(RESOURCE_LOG) << "Could not save resource file" << newFilename;
        return false;
    }

    resource->setFilename(newFilename);
    file.close();

    if (!resource->thumbnailPath().isEmpty()) {
        const PkString thumbnailPath = PkResourceStorage::joinPath(
            saveLocation, resource->thumbnailPath());
        if (!storage.exists(thumbnailPath) &&
            !KisResourceThumbnailCodec::savePng(thumbnailPath, resource->thumbnail())) {
            qCWarning(RESOURCE_LOG) << "Could not save resource thumbnail" << thumbnailPath;
        }
    }

    return true;
}


PkSharedPointer<KisResourceStorage::ResourceIterator> KisResourceStorage::ResourceIterator::versions() const
{
    struct DumbIterator : public ResourceIterator
    {
    public:
        DumbIterator(const ResourceIterator *parent)
            : m_parent(parent)
        {
        }

        bool hasNext() const override {
            return !m_isStarted;
        }

        void next() override {
            KIS_SAFE_ASSERT_RECOVER_NOOP(!m_isStarted);
            m_isStarted = true;
        }
        PkString url() const override
        {
            return m_parent->url();
        }

        PkString type() const override
        {
            return m_parent->type();
        }

        PkDateTime lastModified() const override
        {
            return m_parent->lastModified();
        }

        int guessedVersion() const override
        {
            return m_parent->guessedVersion();
        }

        PkSharedPointer<KisResourceStorage::ResourceIterator> versions() const override
        {
            return PkSharedPointer<KisResourceStorage::ResourceIterator>(new DumbIterator(m_parent));
        }

    protected:
        KoResourceSP resourceImpl() const override
        {
            return m_parent->resource();
        }

    private:
        bool m_isStarted = false;
        const ResourceIterator *m_parent;
    };

    return PkSharedPointer<KisResourceStorage::ResourceIterator>(new DumbIterator(this));
}

KoResourceSP KisResourceStorage::ResourceIterator::resource() const
{
    if (m_cachedResource && m_cachedResourceUrl == url()) {
        return m_cachedResource;
    }

    m_cachedResource = resourceImpl();
    m_cachedResourceUrl = url();

    return m_cachedResource;
}

KisVersionedStorageIterator::KisVersionedStorageIterator(const PkVector<VersionedResourceEntry> &entries, KisStoragePlugin *_q)
    : q(_q)
    , m_entries(entries)
    , m_begin(m_entries.cbegin())
    , m_end(m_entries.cend())

{
    //    ENTER_FUNCTION() << ppVar(std::distance(m_begin, m_end));
    //    for (auto it = m_begin; it != m_end; ++it) {
    //        qDebug() << ppVar(it->filename) << ppVar(it->guessedVersion);
    //    }
}

KisVersionedStorageIterator::KisVersionedStorageIterator(const PkVector<VersionedResourceEntry> &entries,
                                                         PkVector<VersionedResourceEntry>::const_iterator begin,
                                                         PkVector<VersionedResourceEntry>::const_iterator end,
                                                         KisStoragePlugin *_q)
    : q(_q)
    , m_entries(entries)
    , m_begin(begin)
    , m_end(end)
{
//    ENTER_FUNCTION() << ppVar(std::distance(m_begin, m_end));
//    for (auto it = m_begin; it != m_end; ++it) {
//        qDebug() << ppVar(it->filename) << ppVar(it->guessedVersion);
//    }
}

bool KisVersionedStorageIterator::hasNext() const
{
    return (!m_isStarted && m_begin != m_end) ||
            (m_isStarted && std::next(m_it) != m_end);
}

void KisVersionedStorageIterator::next()
{

    if (!m_isStarted) {
        m_isStarted = true;
        m_it = m_begin;
    } else {
        ++m_it;
    }

    KIS_SAFE_ASSERT_RECOVER_RETURN(m_it != m_end);

    auto nextChunk = std::upper_bound(m_it, m_end, *m_it, VersionedResourceEntry::KeyLess());
    m_chunkStart = m_it;
    m_it = std::prev(nextChunk);
}

PkString KisVersionedStorageIterator::url() const
{
    return m_it->resourceType + "/" + m_it->filename;
}

PkString KisVersionedStorageIterator::type() const
{
    return m_it->resourceType;
}

PkDateTime KisVersionedStorageIterator::lastModified() const
{
    return m_it->lastModified;
}

KoResourceSP KisVersionedStorageIterator::resourceImpl() const
{
    return q->resource(url());
}

int KisVersionedStorageIterator::guessedVersion() const
{
    return m_it->guessedVersion;
}

PkSharedPointer<KisResourceStorage::ResourceIterator> KisVersionedStorageIterator::versions() const
{
    struct VersionsIterator : public KisVersionedStorageIterator
    {
        VersionsIterator(const PkVector<VersionedResourceEntry> &entries,
                         PkVector<VersionedResourceEntry>::const_iterator begin,
                         PkVector<VersionedResourceEntry>::const_iterator end,
                         KisStoragePlugin *_q)
            : KisVersionedStorageIterator(entries, begin, end, _q)
        {
        }

        void next() override {
            if (!m_isStarted) {
                m_isStarted = true;
                m_it = m_begin;
            } else {
                ++m_it;
            }
        }

        PkSharedPointer<KisResourceStorage::ResourceIterator> versions() const override{
            return PkSharedPointer<KisResourceStorage::ResourceIterator>(
                new VersionsIterator(m_entries, m_it, std::next(m_it), q));
        }
    };

    return PkSharedPointer<KisResourceStorage::ResourceIterator>(
        new VersionsIterator(m_entries, m_chunkStart, std::next(m_it), q));
}
