/*
 * SPDX-FileCopyrightText: 2014 Victor Lafon metabolic.ewilan @hotmail.fr
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoResourceBundle.h"

#include <KisMimeDatabase.h>
#include <KoMD5Generator.h>
#include <KoStore.h>
#include <KoXmlWriter.h>
#include <PkAuxTypes.h>
#include <PkDateTime.h>
#include <PkMemoryStream.h>
#include <PkStream.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <KritaVersionWrapper.h>

#include "KisGlobalResourcesInterface.h"
#include "KisResourceLoaderRegistry.h"
#include "KisResourceModel.h"
#include "KisResourceStorage.h"
#include "KisResourceThumbnailCodec.h"
#include "ResourceDebug.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace {

PkByteArray bundleMimeType()
{
    static constexpr char mime[] = "application/x-krita-resourcebundle";
    return PkByteArray(mime, static_cast<int>(sizeof(mime) - 1));
}

PkByteArray readAllBytes(PkStream *stream)
{
    if (!stream) {
        return PkByteArray();
    }
    std::vector<char> bytes;
    char chunk[8192];
    for (PkStream::pk_int64 count = 0;
         (count = stream->read(chunk, sizeof(chunk))) > 0;) {
        bytes.insert(bytes.end(), chunk, chunk + count);
    }
    if (bytes.empty() ||
        bytes.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
        return PkByteArray();
    }
    return PkByteArray(bytes.data(), static_cast<int>(bytes.size()));
}

PkString currentBundleDate()
{
    const PkDate date = PkDate::currentDate();
    char text[32];
    std::snprintf(text, sizeof(text), "%02d/%02d/%04d",
                  date.day(), date.month(), date.year());
    return PkString(text);
}

PkString fileNameOnly(const PkString &path)
{
    std::string utf8 = path.PkToUtf8();
    std::replace(utf8.begin(), utf8.end(), '\\', '/');
    const std::string filename = std::filesystem::u8path(utf8).filename().u8string();
    return PkString::PkFromUtf8(filename.data(), static_cast<int>(filename.size()));
}

PkString pathAfterFirstComponent(const PkString &path)
{
    std::string value = path.PkToUtf8();
    const std::size_t separator = value.find_first_of("/\\");
    if (separator == std::string::npos || separator + 1 >= value.size()) {
        return PkString();
    }
    const std::string relative = value.substr(separator + 1);
    return PkString::PkFromUtf8(relative.data(), static_cast<int>(relative.size()));
}

bool writeBytes(KoStore *store, const PkByteArray &bytes)
{
    return store &&
        store->write(bytes) == static_cast<PkStream::pk_int64>(bytes.size());
}

bool saveResourceToStore(const PkString &filename,
                         KoResourceSP resource,
                         KoStore *store,
                         const PkString &resourceType,
                         KisResourceModel &model)
{
    if (!resource || !resource->valid() || !store || store->bad()) {
        qCWarning(RESOURCE_LOG) << "Cannot save invalid bundle resource";
        return false;
    }

    PkMemoryStream buffer;
    if (!buffer.open(PkStream::ReadWrite) ||
        !model.exportResource(resource, &buffer)) {
        qCWarning(RESOURCE_LOG) << "Cannot serialize bundle resource"
                                << resource->name();
        return false;
    }

    const PkString storePath = resourceType + PkString("/") + filename;
    if (!store->open(storePath)) {
        qCWarning(RESOURCE_LOG) << "Could not open bundle entry" << storePath;
        return false;
    }
    const PkStream::pk_int64 written = store->write(buffer.data(), buffer.size());
    const bool resourceClosed = store->close();
    if (written != buffer.size() || !resourceClosed) {
        qCWarning(RESOURCE_LOG) << "Could not write bundle entry" << storePath;
        return false;
    }

    if (!resource->thumbnailPath().isEmpty()) {
        KoResourceSP clonedResource = resource->clone();
        clonedResource->setFilename(filename);
        const PkString thumbnailPath = resourceType + PkString("/") +
            clonedResource->thumbnailPath();
        if (!store->open(thumbnailPath)) {
            qCWarning(RESOURCE_LOG) << "Could not open bundle thumbnail"
                                    << thumbnailPath;
            return false;
        }
        const PkByteArray encoded =
            KisResourceThumbnailCodec::encodePng(resource->thumbnail());
        const bool thumbnailWritten = !encoded.isEmpty() && writeBytes(store, encoded);
        const bool thumbnailClosed = store->close();
        if (!thumbnailWritten || !thumbnailClosed) {
            qCWarning(RESOURCE_LOG) << "Could not write bundle thumbnail"
                                    << thumbnailPath;
            return false;
        }
    }
    return true;
}

} // namespace

KoResourceBundle::KoResourceBundle(const PkString &fileName)
    : m_filename(fileName)
    , m_bundleVersion("1")
{
    m_metadata[KisResourceStorage::s_meta_generator] =
        PkString("Krita (") + KritaVersionWrapper::versionString(true) + PkString(")");
}

KoResourceBundle::~KoResourceBundle() = default;

PkString KoResourceBundle::defaultFileExtension() const
{
    return PkString(".bundle");
}

bool KoResourceBundle::load()
{
    if (m_filename.isEmpty()) {
        return false;
    }
    PkScopedPointer<KoStore> resourceStore(KoStore::createStore(
        m_filename, KoStore::Read, bundleMimeType(), KoStore::Zip));
    return resourceStore && !resourceStore->bad() &&
        loadFromStore(resourceStore.data());
}

bool KoResourceBundle::loadFromDevice(PkStream *device)
{
    (void)device;
    return false;
}

bool KoResourceBundle::loadFromStore(KoStore *resourceStore)
{
    if (!resourceStore || resourceStore->bad()) {
        qCWarning(RESOURCE_LOG) << "Could not open bundle" << m_filename;
        return false;
    }

    m_metadata.clear();
    m_bundletags.clear();

    if (!resourceStore->open(PkString("META-INF/manifest.xml"))) {
        qCWarning(RESOURCE_LOG) << "Could not load bundle manifest";
        return false;
    }
    const bool manifestLoaded = m_manifest.load(resourceStore->device());
    const bool manifestClosed = resourceStore->close();
    if (!manifestLoaded || !manifestClosed) {
        qCWarning(RESOURCE_LOG) << "Could not parse bundle manifest"
                                << m_filename;
        return false;
    }

    for (KoResourceBundleManifest::ResourceReference reference :
         m_manifest.files()) {
        if (!resourceStore->hasFile(reference.resourcePath)) {
            m_manifest.removeResource(reference);
            qCWarning(RESOURCE_LOG) << "Bundle entry is missing"
                                    << reference.resourcePath;
        }
    }

    if (!readMetaData(resourceStore)) {
        return false;
    }
    const bool versionFound =
        m_metadata.contains(KisResourceStorage::s_meta_version);

    if (resourceStore->open(PkString("preview.png"))) {
        const PkByteArray data = readAllBytes(resourceStore->device());
        m_thumbnail = KisResourceThumbnailCodec::decodePng(data);
        resourceStore->close();
    } else {
        qCWarning(RESOURCE_LOG) << "Could not open bundle preview";
    }

    if (!versionFound) {
        m_metadata.insert(KisResourceStorage::s_meta_version, PkString("1"));
    }
    return true;
}

bool KoResourceBundle::save()
{
    if (m_filename.isEmpty()) {
        return false;
    }

    if (metaData(KisResourceStorage::s_meta_creation_date).isEmpty()) {
        setMetaData(KisResourceStorage::s_meta_creation_date,
                    currentBundleDate());
    }
    setMetaData(KisResourceStorage::s_meta_dc_date, currentBundleDate());

    PkScopedPointer<KoStore> store(KoStore::createStore(
        m_filename, KoStore::Write, bundleMimeType(), KoStore::Zip));
    if (!store || store->bad()) {
        return false;
    }

    for (const PkString &resourceType : m_manifest.types()) {
        KisResourceModel model(resourceType);
        model.setResourceFilter(KisResourceModel::ShowAllResources);
        for (const KoResourceBundleManifest::ResourceReference &reference :
             m_manifest.files(resourceType)) {
            KoResourceSP resource;
            if (reference.resourceId >= 0) {
                resource = model.resourceForId(reference.resourceId);
            }
            if (!resource) {
                const PkVector<KoResourceSP> candidates =
                    model.resourcesForMD5(reference.md5sum);
                if (!candidates.isEmpty()) {
                    resource = candidates.first();
                }
            }
            if (!resource) {
                const PkVector<KoResourceSP> candidates =
                    model.resourcesForFilename(fileNameOnly(reference.resourcePath));
                if (!candidates.isEmpty()) {
                    resource = candidates.first();
                }
            }
            if (!resource) {
                qCWarning(RESOURCE_LOG) << "Could not find bundle resource"
                                        << resourceType
                                        << reference.resourcePath;
                continue;
            }
            if (!saveResourceToStore(reference.filenameInBundle,
                                     resource,
                                     store.data(),
                                     resourceType,
                                     model)) {
                qCWarning(RESOURCE_LOG) << "Could not save bundle resource"
                                        << resource->name();
            }
        }
    }

    if (!m_thumbnail.isNull()) {
        const PkByteArray preview =
            KisResourceThumbnailCodec::encodePng(m_thumbnail);
        if (preview.isEmpty() ||
            !store->open(PkString("preview.png")) ||
            !writeBytes(store.data(), preview) ||
            !store->close()) {
            qCWarning(RESOURCE_LOG) << "Could not write bundle preview";
            return false;
        }
    }

    saveManifest(store);
    saveMetadata(store);
    return store->finalize();
}

bool KoResourceBundle::saveToDevice(PkStream *device) const
{
    (void)device;
    return false;
}

void KoResourceBundle::setMetaData(const PkString &key,
                                   const PkString &value)
{
    m_metadata.insert(key, value);
}

PkString KoResourceBundle::metaData(const PkString &key,
                                    const PkString &defaultValue) const
{
    return m_metadata.value(key, defaultValue);
}

void KoResourceBundle::addResource(PkString resourceType,
                                   PkString filePath,
                                   PkVector<KisTagSP> fileTagList,
                                   const PkString &md5sum,
                                   int resourceId,
                                   const PkString &filenameInBundle)
{
    PkStringList tags;
    for (const KisTagSP &tag : fileTagList) {
        if (tag) {
            tags.append(tag->url());
        }
    }
    m_manifest.addResource(resourceType, filePath, tags, md5sum,
                           resourceId, filenameInBundle);
}

PkList<PkString> KoResourceBundle::getTagsList()
{
    return m_bundletags.values();
}

PkStringList KoResourceBundle::resourceTypes() const
{
    return m_manifest.types();
}

void KoResourceBundle::setThumbnail(PkImage image)
{
    if (!image.isNull()) {
        m_thumbnail = image.scaled(PkSize(256, 256),
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        return;
    }
    m_thumbnail = PkImage(256, 256, PkImage::Format_ARGB32);
    m_thumbnail.fill(0xffff0000u);
}

void KoResourceBundle::writeMeta(const PkString &metaTag,
                                 KoXmlWriter *writer)
{
    if (!writer || !m_metadata.contains(metaTag)) {
        return;
    }
    const std::string tag = metaTag.PkToUtf8();
    writer->startElement(tag.c_str());
    writer->addTextNode(m_metadata.value(metaTag));
    writer->endElement();
}

void KoResourceBundle::writeUserDefinedMeta(const PkString &metaTag,
                                            KoXmlWriter *writer)
{
    if (!writer || !m_metadata.contains(metaTag)) {
        return;
    }
    writer->startElement("meta:meta-userdefined");
    writer->addAttribute("meta:name", metaTag);
    writer->addAttribute("meta:value", m_metadata.value(metaTag));
    writer->endElement();
}

bool KoResourceBundle::readMetaData(KoStore *resourceStore)
{
    if (!resourceStore || !resourceStore->open(PkString("meta.xml"))) {
        qCWarning(RESOURCE_LOG) << "Could not open bundle metadata"
                                << m_filename;
        return false;
    }

    PkXmlDocument document;
    if (!document.setContent(resourceStore->device())) {
        qCWarning(RESOURCE_LOG) << "Could not parse bundle metadata"
                                << m_filename;
        resourceStore->close();
        return false;
    }

    const PkXmlElement root = document.documentElement();
    if (root.tagName() != PkString("meta:meta")) {
        qCWarning(RESOURCE_LOG) << "Unexpected bundle metadata root"
                                << root.tagName();
        resourceStore->close();
        return false;
    }

    for (PkXmlElement element = root.firstChildElement();
         !element.isNull();
         element = element.nextSiblingElement()) {
        PkString name = element.tagName();
        PkString value = element.text();
        if (name == PkString("meta:meta-userdefined")) {
            name = element.attribute(PkString("meta:name"));
            value = element.attribute(PkString("meta:value"));
            if (name == PkString("tag")) {
                m_bundletags.insert(value);
                continue;
            }
            if (name != PkString("email") &&
                name != PkString("license") &&
                name != PkString("website")) {
                qCWarning(RESOURCE_LOG) << "Unrecognized bundle metadata"
                                        << name;
            }
            m_metadata.insert(name, value);
            name = PkString("meta:") + name;
        } else if (name == PkString("cd:creator")) {
            name = PkString("dc:creator");
        }
        if (!m_metadata.contains(name)) {
            m_metadata.insert(name, value);
        }
    }

    return resourceStore->close();
}

void KoResourceBundle::saveMetadata(PkScopedPointer<KoStore> &store)
{
    if (!store || !store->open(PkString("meta.xml"))) {
        qCWarning(RESOURCE_LOG) << "Could not open bundle metadata for writing";
        return;
    }

    PkMemoryStream buffer;
    buffer.open(PkStream::WriteOnly);
    KoXmlWriter writer(&buffer);
    writer.startDocument("office:document-meta");
    writer.startElement("meta:meta");
    writer.addAttribute("xmlns:meta", KisResourceStorage::s_xmlns_meta);
    writer.addAttribute("xmlns:dc", KisResourceStorage::s_xmlns_dc);

    writeMeta(KisResourceStorage::s_meta_generator, &writer);
    const std::string versionTag =
        KisResourceStorage::s_meta_version.PkToUtf8();
    writer.startElement(versionTag.c_str());
    writer.addTextNode(m_bundleVersion);
    writer.endElement();

    writeMeta(KisResourceStorage::s_meta_author, &writer);
    writeMeta(KisResourceStorage::s_meta_title, &writer);
    writeMeta(KisResourceStorage::s_meta_description, &writer);
    writeMeta(KisResourceStorage::s_meta_initial_creator, &writer);
    writeMeta(KisResourceStorage::s_meta_creator, &writer);
    writeMeta(KisResourceStorage::s_meta_creation_date, &writer);
    writeMeta(KisResourceStorage::s_meta_dc_date, &writer);
    writeMeta(KisResourceStorage::s_meta_email, &writer);
    writeMeta(KisResourceStorage::s_meta_license, &writer);
    writeMeta(KisResourceStorage::s_meta_website, &writer);

    writeUserDefinedMeta(PkString("email"), &writer);
    writeUserDefinedMeta(PkString("license"), &writer);
    writeUserDefinedMeta(PkString("website"), &writer);

    for (const PkString &tag : m_bundletags) {
        writer.startElement("meta:meta-userdefined");
        writer.addAttribute("meta:name", "tag");
        writer.addAttribute("meta:value", tag);
        writer.endElement();
    }

    writer.endElement();
    writer.endDocument();
    if (store->write(buffer.data(), buffer.size()) != buffer.size()) {
        qCWarning(RESOURCE_LOG) << "Could not write bundle metadata";
    }
    store->close();
}

void KoResourceBundle::saveManifest(PkScopedPointer<KoStore> &store)
{
    if (!store || !store->open(PkString("META-INF/manifest.xml"))) {
        qCWarning(RESOURCE_LOG) << "Could not open bundle manifest for writing";
        return;
    }
    PkMemoryStream buffer;
    buffer.open(PkStream::WriteOnly);
    if (!m_manifest.save(&buffer) ||
        store->write(buffer.data(), buffer.size()) != buffer.size()) {
        qCWarning(RESOURCE_LOG) << "Could not write bundle manifest";
    }
    store->close();
}

int KoResourceBundle::resourceCount() const
{
    return m_manifest.files().count();
}

KoResourceBundleManifest &KoResourceBundle::manifest()
{
    return m_manifest;
}

KoResourceSP KoResourceBundle::resource(const PkString &resourceType,
                                        const PkString &filepath)
{
    const PkString mime = KisMimeDatabase::mimeTypeForFile(filepath);
    KisResourceLoaderBase *loader =
        KisResourceLoaderRegistry::instance()->loader(resourceType, mime);
    if (!loader) {
        qCWarning(RESOURCE_LOG) << "Could not create bundle resource loader"
                                << resourceType << filepath << mime;
        return KoResourceSP();
    }

    const PkString name = pathAfterFirstComponent(filepath);
    if (name.isEmpty()) {
        return KoResourceSP();
    }
    KoResourceSP result = loader->create(name);
    return loadResource(result) ? result : KoResourceSP();
}

bool KoResourceBundle::exportResource(const PkString &resourceType,
                                      const PkString &fileName,
                                      PkStream *device)
{
    if (m_filename.isEmpty() || !device) {
        return false;
    }
    PkScopedPointer<KoStore> resourceStore(KoStore::createStore(
        m_filename, KoStore::Read, bundleMimeType(), KoStore::Zip));
    if (!resourceStore || resourceStore->bad()) {
        return false;
    }

    const PkString filePath = resourceType + PkString("/") + fileName;
    if (!resourceStore->open(filePath)) {
        qCWarning(RESOURCE_LOG) << "Could not open bundle entry" << filePath;
        return false;
    }

    char chunk[8192];
    bool result = true;
    for (PkStream::pk_int64 count = 0;
         (count = resourceStore->device()->read(chunk, sizeof(chunk))) > 0;) {
        if (device->write(chunk, count) != count) {
            result = false;
            break;
        }
    }
    return resourceStore->close() && result;
}

bool KoResourceBundle::loadResource(KoResourceSP resource)
{
    if (m_filename.isEmpty() || !resource) {
        return false;
    }
    const PkString resourceType = resource->resourceType().first;
    PkScopedPointer<KoStore> resourceStore(KoStore::createStore(
        m_filename, KoStore::Read, bundleMimeType(), KoStore::Zip));
    if (!resourceStore || resourceStore->bad()) {
        return false;
    }

    const PkString fileName = resourceType + PkString("/") +
        resource->filename();
    if (!resourceStore->open(fileName)) {
        qCWarning(RESOURCE_LOG) << "Could not open bundle resource"
                                << fileName;
        return false;
    }
    if (!resource->loadFromDevice(resourceStore->device(),
                                  KisGlobalResourcesInterface::instance())) {
        resourceStore->close();
        qCWarning(RESOURCE_LOG) << "Could not load bundle resource"
                                << fileName;
        return false;
    }
    resourceStore->close();

    if ((resource->image().isNull() || resource->thumbnail().isNull()) &&
        !resource->thumbnailPath().isEmpty()) {
        const PkString thumbnailPath = resourceType + PkString("/") +
            resource->thumbnailPath();
        if (!resourceStore->open(thumbnailPath)) {
            qCWarning(RESOURCE_LOG) << "Could not open bundle thumbnail"
                                    << thumbnailPath;
            return false;
        }
        const PkImage thumbnail = KisResourceThumbnailCodec::decodePng(
            readAllBytes(resourceStore->device()));
        resourceStore->close();
        if (thumbnail.isNull()) {
            return false;
        }
        resource->setImage(thumbnail);
        resource->updateThumbnail();
    }
    return true;
}

PkString KoResourceBundle::resourceMd5(const PkString &url)
{
    if (m_filename.isEmpty()) {
        return PkString();
    }
    PkScopedPointer<KoStore> resourceStore(KoStore::createStore(
        m_filename, KoStore::Read, bundleMimeType(), KoStore::Zip));
    if (!resourceStore || resourceStore->bad() ||
        !resourceStore->open(url)) {
        return PkString();
    }
    const PkString result =
        KoMD5Generator::generateHash(resourceStore->device());
    resourceStore->close();
    return result;
}

PkImage KoResourceBundle::image() const
{
    return m_thumbnail;
}

PkString KoResourceBundle::filename() const
{
    return m_filename;
}
