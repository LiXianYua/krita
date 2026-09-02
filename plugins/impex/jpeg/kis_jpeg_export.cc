/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisDocument.h>

#include "kis_jpeg_export.h"

#include "../kis_impex_static_registration.h"
#include <PkColor.h>
#include <PkString.h>
#include <PkStringList.h>
#include <PkScopedPointer.h>
#include <KoColorSpace.h>
#include <KoColorProfile.h>
#include <KoColorSpaceConstants.h>
#include <KoColorSpaceRegistry.h>

#include <KisImportExportManager.h>
#include <KoDocumentInfo.h>
#include <kis_image.h>
#include <kis_group_layer.h>
#include <kis_paint_layer.h>
#include <kis_paint_device.h>
#include <kis_properties_configuration.h>
#include <kis_meta_data_store.h>
#include <kis_meta_data_entry.h>
#include <kis_meta_data_value.h>
#include <kis_meta_data_schema.h>
#include <kis_meta_data_schema_registry.h>
#include <kis_meta_data_filter_registry_model.h>
#include <kis_exif_info_visitor.h>
#include <generator/kis_generator_layer.h>
#include <KisExportCheckRegistry.h>
#include "kis_jpeg_converter.h"

class KisExternalLayer;

extern "C" KRITAIMPEX_EXPORT bool registerKisJPEGExportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {}, {PkString("image/jpeg")}, 1,
        []() -> KisImportExportFilter * { return new KisJPEGExport(nullptr, PkVariantList()); });
}

KisJPEGExport::KisJPEGExport(PkObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisJPEGExport::~KisJPEGExport()
{
}

KisImportExportErrorCode KisJPEGExport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    KisImageSP image = document->savingImage();
    Q_CHECK_PTR(image);

    // An extra option to pass to the config widget to set the state correctly, this isn't saved
    const KoColorSpace* cs = image->projection()->colorSpace();
    bool sRGB = cs->profile()->name().toLower().contains("srgb");
    configuration->setProperty("is_sRGB", sRGB);

    KisJPEGOptions options;
    options.progressive = configuration->getBool("progressive", false);
    options.quality = configuration->getInt("quality", 80);
    options.forceSRGB = configuration->getBool("forceSRGB", false);
    options.saveProfile = configuration->getBool("saveProfile", true);
    options.optimize = configuration->getBool("optimize", true);
    options.smooth = configuration->getInt("smoothing", 0);
    options.baseLineJPEG = configuration->getBool("baseline", true);
    options.subsampling = configuration->getInt("subsampling", 0);
    options.exif = configuration->getBool("exif", true);
    options.iptc = configuration->getBool("iptc", true);
    options.xmp = configuration->getBool("xmp", true);
    KoColor c(KoColorSpaceRegistry::instance()->rgb8());
    c.fromQColor(PkColor(255, 255, 255));
    options.transparencyFillColor = configuration->getColor("transparencyFillcolor", c).toQColor();
    KisMetaData::FilterRegistryModel m;
    PkStringList enabledFilters;
    for (const PkString &filter : configuration->getString("filters").split(u',')) {
        enabledFilters << filter;
    }
    m.setEnabledFilters(enabledFilters);
    options.filters = m.enabledFilters();
    options.storeAuthor = configuration->getBool("storeAuthor", false);
    options.storeDocumentMetaData = configuration->getBool("storeMetaData", false);

    KisPaintDeviceSP pd = new KisPaintDevice(*image->projection());

    KisJPEGConverter kpc(document, batchMode());
    KisPaintLayerSP l = new KisPaintLayer(image, "projection", OPACITY_OPAQUE_U8, pd);

    KisExifInfoVisitor exivInfoVisitor;
    exivInfoVisitor.visit(image->rootLayer().data());

    PkScopedPointer<KisMetaData::Store> metaDataStore;
    if (exivInfoVisitor.metaDataCount() == 1) {
        metaDataStore.reset(new KisMetaData::Store(*exivInfoVisitor.exifInfo()));
    }
    else {
        metaDataStore.reset(new KisMetaData::Store());
    }

    //add extra meta-data here
    const KisMetaData::Schema* dcSchema = KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::DublinCoreSchemaUri);
    Q_ASSERT(dcSchema);
    if (options.storeDocumentMetaData) {
        PkString title = document->documentInfo()->aboutInfo("title");
        if (!title.isEmpty()) {
            if (metaDataStore->containsEntry("title")) {
                metaDataStore->removeEntry("title");
            }
            metaDataStore->addEntry(KisMetaData::Entry(dcSchema, "title", KisMetaData::Value(PkVariant(title))));
        }
        PkString description = document->documentInfo()->aboutInfo("subject");
        if (description.isEmpty()) {
            description = document->documentInfo()->aboutInfo("abstract");
        }
        if (!description.isEmpty()) {
            PkString keywords = document->documentInfo()->aboutInfo("keyword");
            if (!keywords.isEmpty()) {
                description = description + " keywords: " + keywords;
            }
            if (metaDataStore->containsEntry("description")) {
                metaDataStore->removeEntry("description");
            }
            metaDataStore->addEntry(KisMetaData::Entry(dcSchema, "description", KisMetaData::Value(PkVariant(description))));
        }
        PkString license = document->documentInfo()->aboutInfo("license");
        if (!license.isEmpty()) {
            if (metaDataStore->containsEntry("rights")) {
                metaDataStore->removeEntry("rights");
            }
            metaDataStore->addEntry(KisMetaData::Entry(dcSchema, "rights", KisMetaData::Value(PkVariant(license))));
        }
        PkString date = document->documentInfo()->aboutInfo("date");
        if (!date.isEmpty() && !metaDataStore->containsEntry("rights")) {
            metaDataStore->addEntry(KisMetaData::Entry(dcSchema, "date", KisMetaData::Value(PkVariant(date))));
        }
    }
    if (options.storeAuthor) {
        PkString author = document->documentInfo()->authorInfo("creator");
        if (!author.isEmpty()) {
            if (!document->documentInfo()->authorContactInfo().isEmpty()) {
                PkString contact = document->documentInfo()->authorContactInfo().at(0);
                if (!contact.isEmpty()) {
                    author = author+"("+contact+")";
                }
            }
            if (metaDataStore->containsEntry("creator")) {
                metaDataStore->removeEntry("creator");
            }
            metaDataStore->addEntry(KisMetaData::Entry(dcSchema, "creator", KisMetaData::Value(PkVariant(author))));
        }
    }

    KisImportExportErrorCode res = kpc.buildFile(io, l, options, metaDataStore.data());
    return res;
}

KisPropertiesConfigurationSP KisJPEGExport::defaultConfiguration(const PkByteArray &/*from*/, const PkByteArray &/*to*/) const
{
    KisPropertiesConfigurationSP cfg = new KisPropertiesConfiguration();
    cfg->setProperty("progressive", false);
    cfg->setProperty("quality", 80);
    cfg->setProperty("forceSRGB", false);
    cfg->setProperty("saveProfile", true);
    cfg->setProperty("optimize", true);
    cfg->setProperty("smoothing", 0);
    cfg->setProperty("baseline", true);
    cfg->setProperty("subsampling", 0);
    cfg->setProperty("exif", true);
    cfg->setProperty("iptc", true);
    cfg->setProperty("xmp", true);
    cfg->setProperty("storeAuthor", false);
    cfg->setProperty("storeMetaData", false);

    KoColor fill_color(KoColorSpaceRegistry::instance()->rgb8());
    fill_color = KoColor();
    fill_color.fromQColor(PkColor(255, 255, 255));
    PkVariant v;
    v.setValue(fill_color);

    cfg->setProperty("transparencyFillcolor", v);
    cfg->setProperty("filters", "");

    return cfg;
}

void KisJPEGExport::initializeCapabilities()
{
    addCapability(KisExportCheckRegistry::instance()->get("sRGBProfileCheck")->create(KisExportCheckBase::SUPPORTED));
    addCapability(KisExportCheckRegistry::instance()->get("ExifCheck")->create(KisExportCheckBase::SUPPORTED));

    PkList<std::pair<KoID, KoID> > supportedColorModels;
    supportedColorModels << std::pair<KoID, KoID>()
            << std::pair<KoID, KoID>(RGBAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(GrayAColorModelID, Integer8BitsColorDepthID)
            << std::pair<KoID, KoID>(CMYKAColorModelID, Integer8BitsColorDepthID);
    addSupportedColorModels(supportedColorModels, "JPEG");
}
