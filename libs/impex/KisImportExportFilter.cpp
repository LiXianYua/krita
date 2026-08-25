/*
    This file is part of the KDE libraries

    SPDX-FileCopyrightText: 2001 Werner Trobin <trobin@kde.org>
    SPDX-FileCopyrightText: 2002 Werner Trobin <trobin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KisImportExportFilter.h"

#include <kis_debug.h>
#include <kis_assert.h>
#include "KoUpdater.h"
#include <KoStore.h>
#include <PkFileStream.h>
#include <PkScopedPointer.h>

#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KisExportCheckBase.h>
#include <KisExportCheckRegistry.h>

#include <unistd.h>
#include <filesystem>
#include <string>
#include <algorithm>

#include "KisImportExportBackend.h"
KisImportExportUiServices *kisImportExportUiServices();

const PkString KisImportExportFilter::ImageContainsTransparencyTag = "ImageContainsTransparency";
const PkString KisImportExportFilter::ColorModelIDTag = "ColorModelID";
const PkString KisImportExportFilter::ColorDepthIDTag = "ColorDepthID";
const PkString KisImportExportFilter::sRGBTag = "sRGB";
const PkString KisImportExportFilter::CICPPrimariesTag = "CICPCompatiblePrimaries";
const PkString KisImportExportFilter::CICPTransferCharacteristicsTag = "CICPCompatibleTransferFunction";
const PkString KisImportExportFilter::HDRTag = "HDRSupported";

class Q_DECL_HIDDEN KisImportExportFilter::Private
{
public:
    PkPointer<KoUpdater> updater;
    PkByteArray mime;
    PkString filename;
    PkString realFilename;
    bool batchmode;
    KisImportUserFeedbackInterface *importUserFeedBackInterface {nullptr};

    PkMap<PkString, KisExportCheckBase*> capabilities;

    Private()
        : updater(0)
        , batchmode(false)
    {}

    ~Private()
    {
        for (auto it = capabilities.begin(); it != capabilities.end(); ++it) {
            delete it.value();
        }
    }

};


KisImportExportFilter::KisImportExportFilter(PkObject *parent)
    : PkObject(parent)
    , d(new Private)
{
}

KisImportExportFilter::~KisImportExportFilter()
{
    if (d->updater) {
        d->updater->setProgress(100);
    }
    delete d;
}

PkString KisImportExportFilter::filename() const
{
    return d->filename;
}

PkString KisImportExportFilter::realFilename() const
{
    return d->realFilename;
}

bool KisImportExportFilter::batchMode() const
{
    return d->batchmode;
}

KisImportUserFeedbackInterface *KisImportExportFilter::importUserFeedBackInterface() const
{
    return d->importUserFeedBackInterface;
}

void KisImportExportFilter::setBatchMode(bool batchmode)
{
    d->batchmode = batchmode;
}

void KisImportExportFilter::setImportUserFeedBackInterface(KisImportUserFeedbackInterface *interface)
{
    d->importUserFeedBackInterface = interface;
}

void KisImportExportFilter::setFilename(const PkString &filename)
{
    d->filename = filename;
}

void KisImportExportFilter::setRealFilename(const PkString &filename)
{
    d->realFilename = filename;
}


void KisImportExportFilter::setMimeType(const PkString &mime)
{
    const std::string utf8 = mime.PkToUtf8();
    d->mime = PkByteArray(utf8.data(), static_cast<int>(utf8.size()));
}

PkByteArray KisImportExportFilter::mimeType() const
{
    return d->mime;
}

KisPropertiesConfigurationSP KisImportExportFilter::defaultConfiguration(const PkByteArray &from, const PkByteArray &to) const
{
    (void)from;
    (void)to;
    return 0;
}

KisPropertiesConfigurationSP KisImportExportFilter::lastSavedConfiguration(const PkByteArray &from, const PkByteArray &to) const
{
    KisPropertiesConfigurationSP cfg = defaultConfiguration(from, to);
    const PkString filterConfig = kisImportExportUiServices() ? kisImportExportUiServices()->exportConfigurationXml(to) : PkString();
    if (cfg && !filterConfig.isEmpty()) {
        cfg->fromXML(filterConfig, false);
    }
    return cfg;
}

PkMap<PkString, KisExportCheckBase *> KisImportExportFilter::exportChecks()
{
    for (auto it = d->capabilities.begin(); it != d->capabilities.end(); ++it) {
        delete it.value();
    }
    d->capabilities.clear();
    initializeCapabilities();
    return d->capabilities;
}

bool KisImportExportFilter::exportSupportsGuides() const
{
    return false;
}

PkString KisImportExportFilter::verify(const PkString &fileName) const
{
    const std::string utf8Name = fileName.PkToUtf8();
    std::error_code ec;

    if (!std::filesystem::exists(utf8Name, ec)) {
        return PkString("%1 does not exist after writing. Try saving again under a different name, in another location.").arg(fileName);
    }

    if (::access(utf8Name.c_str(), R_OK) != 0) {
        return PkString("%1 is not readable").arg(fileName);
    }

    if (std::filesystem::file_size(utf8Name, ec) < 10)  {
        return PkString("%1 is smaller than 10 bytes, it must be corrupt. Try saving again under a different name, in another location.").arg(fileName);
    }

    PkFileStream f(fileName);
    if (!f.open(PkStream::ReadOnly)) {
        return PkString("%1 could not be opened").arg(fileName);
    }
    PkByteArray ba;
    const PkStream::pk_int64 fileSize = f.size();
    const PkStream::pk_int64 bytesToRead = std::min(fileSize, static_cast<PkStream::pk_int64>(1000));
    ba.resize(static_cast<int>(bytesToRead));
    const PkStream::pk_int64 n = f.read(ba.data(), bytesToRead);
    if (n >= 0) {
        ba.resize(static_cast<int>(n));
    }

    bool found = false;
    for(int i = 0; i < ba.size(); ++i) {
        if (ba.data()[i] > 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        return PkString("%1 has only zero bytes in the first 1000 bytes, it's probably corrupt. Try saving again under a different name, in another location.").arg(fileName);
    }

    return PkString();
}

void KisImportExportFilter::setUpdater(PkPointer<KoUpdater> updater)
{
    d->updater = updater;
}

PkPointer<KoUpdater> KisImportExportFilter::updater()
{
    return d->updater;
}

void KisImportExportFilter::setProgress(int value)
{
    if (d->updater) {
        d->updater->setValue(value);
    }
}

void KisImportExportFilter::initializeCapabilities()
{
    // XXX: Initialize everything to fully supported?
}

void KisImportExportFilter::addCapability(KisExportCheckBase *capability)
{
    d->capabilities[capability->id()] = capability;
}



void KisImportExportFilter::addSupportedColorModels(PkList<std::pair<KoID, KoID> > supportedColorModels, const PkString &name, KisExportCheckBase::Level level)
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(level != KisExportCheckBase::SUPPORTED);
    PkString layerMessage;
    PkString imageMessage;
    PkList<KoID> allColorModels = KoColorSpaceRegistry::instance()->colorModelsList(KoColorSpaceRegistry::AllColorSpaces);
    for (const KoID &colorModelID : allColorModels) {
        PkList<KoID> allColorDepths = KoColorSpaceRegistry::instance()->colorDepthList(colorModelID.id(), KoColorSpaceRegistry::AllColorSpaces);
        for (const KoID &colorDepthID : allColorDepths) {

            KisExportCheckFactory *colorModelCheckFactory =
                    KisExportCheckRegistry::instance()->get(PkString("ColorModelCheck/") + colorModelID.id() + "/" + colorDepthID.id());
            KisExportCheckFactory *colorModelPerLayerCheckFactory =
                    KisExportCheckRegistry::instance()->get(PkString("ColorModelPerLayerCheck/") + colorModelID.id() + "/" + colorDepthID.id());

            if(!colorModelCheckFactory || !colorModelPerLayerCheckFactory) {
                warnKrita << "No factory for" << colorModelID << colorDepthID;
                continue;
            }

            if (supportedColorModels.contains(std::make_pair(colorModelID, colorDepthID))) {
                addCapability(colorModelCheckFactory->create(KisExportCheckBase::SUPPORTED));
                addCapability(colorModelPerLayerCheckFactory->create(KisExportCheckBase::SUPPORTED));
            }
            else {
                if (level == KisExportCheckBase::PARTIALLY) {
                    imageMessage = PkString("%1 cannot save images with color model <b>%2</b> and depth <b>%3</b>. The image will be converted.")
                            .arg(name, colorModelID.name(), colorDepthID.name());

                    layerMessage =
                            PkString("%1 cannot save layers with color model <b>%2</b> and depth <b>%3</b>. The layers will be converted or skipped.")
                            .arg(name, colorModelID.name(), colorDepthID.name());
                }
                else {
                    imageMessage = PkString("%1 cannot save images with color model <b>%2</b> and depth <b>%3</b>. The image will not be saved.")
                            .arg(name, colorModelID.name(), colorDepthID.name());

                    layerMessage =
                            PkString("%1 cannot save layers with color model <b>%2</b> and depth <b>%3</b>. The layers will be skipped.")
                            .arg(name, colorModelID.name(), colorDepthID.name());
                }

                addCapability(colorModelCheckFactory->create(level, imageMessage));
                addCapability(colorModelPerLayerCheckFactory->create(level, layerMessage));
            }
        }
    }
}

PkString KisImportExportFilter::verifyZiPBasedFiles(const PkString &fileName, const PkStringList &filesToCheck) const
{
    const std::string appId = PkString("application/x-krita").PkToUtf8();
    PkScopedPointer<KoStore> store(KoStore::createStore(fileName, KoStore::Read,
        PkByteArray(appId.data(), static_cast<int>(appId.size())), KoStore::Zip));

    if (!store || store->bad()) {
        return PkString("Could not open the saved file %1. Please try to save again in a different location.").arg(fileName);
    }

    for (const PkString &file : filesToCheck) {
        if (!store->hasFile(file)) {
            return PkString("Component %1 is missing in %2. Please try to save again in a different location.").arg(file, fileName);
        }
    }

    return PkString();

}
