/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2003 Patrick Julien <freak@codepimps.org>
    SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
    SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>

    SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <KoResource.h>

#include <string>

#include <PkGlobal.h>
#include <PkFileStream.h>
#include <PkMemoryStream.h>

#include "ResourceDebug.h"
#include "KoMD5Generator.h"
#include "kis_assert.h"

#include "KoResourceLoadResult.h"

namespace {

PkString resourceFileName(const PkString &path)
{
    std::string utf8 = path.PkToUtf8();

    // Resource paths may originate from bundles or Windows callers. Treat
    // both separators as path boundaries regardless of the host platform.
    while (utf8.size() > 1 && (utf8.back() == '/' || utf8.back() == '\\')) {
        utf8.pop_back();
    }

    const std::size_t slash = utf8.find_last_of("/\\");
    if (slash == std::string::npos) {
        return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
    }
    return PkString::PkFromUtf8(utf8.data() + slash + 1,
                                static_cast<int>(utf8.size() - slash - 1));
}

}

struct KoResource::Private {
    int version {-1};
    int resourceId {-1};
    bool valid {false};
    bool active {true};
    bool permanent {false};
    bool modified {false};
    PkString name;
    PkString filename;
    PkString storageLocation;
    PkString md5sum;
    PkImage image;
    PkMap<PkString, PkVariant> metadata;
};

KoResource::KoResource()
    : d(new Private)
{
}

KoResource::KoResource(const PkString& filename)
    : d(new Private)
{
    d->filename = filename;
    d->name = resourceFileName(filename);
}

KoResource::~KoResource()
{
    delete d;
}

KoResource::KoResource(const KoResource &rhs)
    : d(new Private(*rhs.d))
{
}

bool KoResource::load(KisResourcesInterfaceSP resourcesInterface)
{
    PkFileStream file(filename());

    if (!file.open(PkStream::ReadOnly)) {
        qWarning() << "Cannot open resource file for reading" << filename();
        return false;
    }

    if (file.size() == 0) {
        qWarning() << "Resource file is empty: " << filename();
        file.close();
        return false;
    }

    const bool res = loadFromDevice(&file, resourcesInterface);

    if (!res) {
        qWarning() << "Could not load resource file" << filename();
    }

    file.close();

    return res;
}

bool KoResource::save()
{
    if (filename().isEmpty()) return false;

    PkFileStream file(filename());

    if (!file.open(static_cast<PkStream::OpenMode>(PkStream::WriteOnly | PkStream::Truncate))) {
        warnResource << "Can't open file for writing" << filename();
        return false;
    }

    saveToDevice(&file);

    file.close();
    return true;
}

bool KoResource::saveToDevice(PkStream *dev) const
{
    (void)dev;
    return true;
}

PkImage KoResource::image() const
{
    return d->image;
}

void KoResource::updateThumbnail()
{
}

PkImage KoResource::thumbnail() const
{
    return image();
}

PkString KoResource::thumbnailPath() const
{
    return PkString();
}

void KoResource::setImage(const PkImage &image)
{
    d->image = image;
}

PkString KoResource::md5Sum(bool generateIfEmpty) const
{
    // [this assert is disputable] ephemeral resources have no md5
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!isEphemeral(), PkString());

    if (d->md5sum.isEmpty() && generateIfEmpty) {
        // non-serializable resources should always have an externally generated md5
        KIS_SAFE_ASSERT_RECOVER_NOOP(isSerializable());
        debugResource << "No MD5 for" << this << this->name();
        PkMemoryStream buf;
        buf.open(PkStream::WriteOnly);
        saveToDevice(&buf);
        buf.close();

        PkByteArray data;
        if (buf.size() > 0) {
            data = PkByteArray(buf.data(), static_cast<int>(buf.size()));
        }
        const_cast<KoResource*>(this)->setMD5Sum(KoMD5Generator::generateHash(data));
    }
    return d->md5sum;
}

void KoResource::setMD5Sum(const PkString &md5sum)
{
    /// ephemeral resources have no md5, trying to assign
    /// them one is considered an error
    KIS_SAFE_ASSERT_RECOVER_RETURN(!isEphemeral());

    if (valid()) {
        Q_ASSERT(!md5sum.isEmpty());
    }
    d->md5sum = md5sum;
}

PkString KoResource::filename() const
{
    return d->filename;
}

void KoResource::setFilename(const PkString& filename)
{
    d->filename = resourceFileName(filename);
}

PkString KoResource::name() const
{
    return d->name;
}

void KoResource::setName(const PkString& name)
{
    d->name = name;
}

bool KoResource::valid() const
{
    return d->valid;
}

void KoResource::setValid(bool valid)
{
    d->valid = valid;
}

bool KoResource::active() const
{
    return d->active;
}

void KoResource::setActive(bool active)
{
    d->active = active;
}


PkString KoResource::defaultFileExtension() const
{
    return PkString();
}

bool KoResource::permanent() const
{
    return d->permanent;
}

void KoResource::setPermanent(bool permanent)
{
    d->permanent = permanent;
}

int KoResource::resourceId() const
{
    return d->resourceId;
}

PkVector<KoResourceLoadResult> KoResource::requiredResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    PkVector<KoResourceLoadResult> result = linkedResources(globalResourcesInterface);
    result += embeddedResources(globalResourcesInterface);
    return result;
}

PkVector<KoResourceLoadResult> KoResource::linkedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    (void)globalResourcesInterface;
    return {};
}

PkVector<KoResourceLoadResult> KoResource::embeddedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    (void)globalResourcesInterface;
    return {};
}

PkVector<KoResourceLoadResult> KoResource::takeSideLoadedResources(KisResourcesInterfaceSP globalResourcesInterface)
{
    PkVector<KoResourceLoadResult> result = sideLoadedResources(globalResourcesInterface);
    clearSideLoadedResources();
    return result;
}

PkVector<KoResourceLoadResult> KoResource::sideLoadedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    (void)globalResourcesInterface;
    return {};
}

void KoResource::clearSideLoadedResources()
{
}

PkVector<int> KoResource::requiredCanvasResources() const
{
    return {};
}

PkString KoResource::storageLocation() const
{
    return d->storageLocation;
}

void KoResource::setDirty(bool value)
{
    d->modified = value;
}

bool KoResource::isDirty() const
{
    return d->modified;
}

void KoResource::addMetaData(PkString key, PkVariant value)
{
    /**
     * It is responsibility of the resource itself to load all the necessary
     * metadata right on loading in loadFromDevice(). The resource locator will
     * **not** try to load this information from the database. Database only
     * caches metadata.
     *
     * To make sure that metadata is correctly populated in the database,
     * it should be set up right in loadFromDevice().
     */
    d->metadata.insert(key, value);
}

PkMap<PkString, PkVariant> KoResource::metadata() const
{
    return d->metadata;
}

int KoResource::version() const
{
    return d->version;
}

void KoResource::setVersion(int version)
{
    d->version = version;
}

void KoResource::setResourceId(int id)
{
    d->resourceId = id;
}

KoResourceSignature KoResource::signature() const
{
    return KoResourceSignature(resourceType().first, md5Sum(false), filename(), name());
}

bool KoResource::isEphemeral() const
{
    return false;
}

bool KoResource::isSerializable() const
{
    return !isEphemeral();
}

void KoResource::setStorageLocation(const PkString &location)
{
    d->storageLocation = location;
}
