/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "kis_iptc_io.h"

#include <exiv2/iptc.hpp>

#include <cassert>
#include <cstdint>
#include <string>

#include <PkStream.h>
#include <PkStringHash.h>

#include <kis_debug.h>
#include <kis_exiv2_common.h>
#include <kis_meta_data_entry.h>
#include <kis_meta_data_schema.h>
#include <kis_meta_data_schema_registry.h>
#include <kis_meta_data_store.h>
#include <kis_meta_data_value.h>

const char photoshopMarker[] = "Photoshop 3.0\0";
const char photoshopBimId_[] = "8BIM";
const uint16_t photoshopIptc = 0x0404;
const PkByteArray photoshopIptc_((char *)&photoshopIptc, 2);

struct IPTCToKMD {
    PkString exivTag;
    PkString namespaceUri;
    PkString name;
};

static const IPTCToKMD mappings[] = {
    {"Iptc.Application2.City", KisMetaData::Schema::PhotoshopSchemaUri, "City"},
    {"Iptc.Application2.Copyright", KisMetaData::Schema::DublinCoreSchemaUri, "rights"},
    {"Iptc.Application2.CountryName", KisMetaData::Schema::PhotoshopSchemaUri, "Country"},
    {"Iptc.Application2.CountryCode", KisMetaData::Schema::IPTCSchemaUri, "CountryCode"},
    {"Iptc.Application2.Byline", KisMetaData::Schema::DublinCoreSchemaUri, "creator"},
    {"Iptc.Application2.BylineTitle", KisMetaData::Schema::PhotoshopSchemaUri, "AuthorsPosition"},
    {"Iptc.Application2.DateCreated", KisMetaData::Schema::PhotoshopSchemaUri, "DateCreated"},
    {"Iptc.Application2.Caption", KisMetaData::Schema::DublinCoreSchemaUri, "description"},
    {"Iptc.Application2.Writer", KisMetaData::Schema::PhotoshopSchemaUri, "CaptionWriter"},
    {"Iptc.Application2.Headline", KisMetaData::Schema::PhotoshopSchemaUri, "Headline"},
    {"Iptc.Application2.SpecialInstructions", KisMetaData::Schema::PhotoshopSchemaUri, "Instructions"},
    {"Iptc.Application2.ObjectAttribute", KisMetaData::Schema::IPTCSchemaUri, "IntellectualGenre"},
    {"Iptc.Application2.TransmissionReference", KisMetaData::Schema::PhotoshopSchemaUri, "JobID"},
    {"Iptc.Application2.Keywords", KisMetaData::Schema::DublinCoreSchemaUri, "subject"},
    {"Iptc.Application2.SubLocation", KisMetaData::Schema::IPTCSchemaUri, "Location"},
    {"Iptc.Application2.Credit", KisMetaData::Schema::PhotoshopSchemaUri, "Credit"},
    {"Iptc.Application2.ProvinceState", KisMetaData::Schema::PhotoshopSchemaUri, "State"},
    {"Iptc.Application2.Source", KisMetaData::Schema::PhotoshopSchemaUri, "Source"},
    {"Iptc.Application2.Subject", KisMetaData::Schema::IPTCSchemaUri, "SubjectCode"},
    {"Iptc.Application2.ObjectName", KisMetaData::Schema::DublinCoreSchemaUri, "title"},
    {"Iptc.Application2.Urgency", KisMetaData::Schema::PhotoshopSchemaUri, "Urgency"},
    {"Iptc.Application2.Category", KisMetaData::Schema::PhotoshopSchemaUri, "Category"},
    {"Iptc.Application2.SuppCategory", KisMetaData::Schema::PhotoshopSchemaUri, "SupplementalCategory"},
    {"", "", ""} // indicates the end of the array
};

struct KisIptcIO::Private {
    PkHash<PkString, IPTCToKMD> iptcToKMD;
    PkHash<PkString, IPTCToKMD> kmdToIPTC;
};

// ---- Implementation of KisIptcIO ----//
KisIptcIO::KisIptcIO()
    : KisMetaData::IOBackend()
    , d(new Private)
{
}

KisIptcIO::~KisIptcIO()
{
    delete d;
}

void KisIptcIO::initMappingsTable() const
{
    // For some reason, initializing the tables in the constructor makes the it crash
    if (d->iptcToKMD.size() == 0) {
        for (int i = 0; !mappings[i].exivTag.isEmpty(); i++) {
            dbgKrita << "mapping[i] = " << mappings[i].exivTag << " " << mappings[i].namespaceUri << " "
                     << mappings[i].name;
            d->iptcToKMD[mappings[i].exivTag] = mappings[i];
            d->kmdToIPTC[KisMetaData::SchemaRegistry::instance()
                             ->schemaFromUri(mappings[i].namespaceUri)
                             ->generateQualifiedName(mappings[i].name)] = mappings[i];
        }
    }
}

bool KisIptcIO::saveTo(const KisMetaData::Store *store, PkStream *ioDevice, HeaderType headerType) const
{
    PkStringList blockedEntries = PkStringList() << "photoshop:DateCreated";

    initMappingsTable();
    ioDevice->open(PkStream::WriteOnly);
    Exiv2::IptcData iptcData;
    for (const KisMetaData::Entry &entry : *store) {
        if (d->kmdToIPTC.contains(entry.qualifiedName())) {
            if (blockedEntries.contains(entry.qualifiedName())) {
                warnKrita << "skipping" << entry.qualifiedName() << entry.value();
                continue;
            }
            try {
                PkString iptcKeyStr = d->kmdToIPTC[entry.qualifiedName()].exivTag;
                Exiv2::IptcKey iptcKey(iptcKeyStr.PkToUtf8());
                Exiv2::Value *v =
                    kmdValueToExivValue(entry.value(),
                                        Exiv2::IptcDataSets::dataSetType(iptcKey.tag(), iptcKey.record()));

                if (v && v->typeId() != Exiv2::invalidTypeId) {
                    iptcData.add(iptcKey, v);
                }
#if EXIV2_TEST_VERSION(0,28,0)
            } catch (Exiv2::Error &e) {
#else
            } catch (Exiv2::AnyError &e) {
#endif
                dbgMetaData << "exiv error " << e.what();
            }
        }
    }
#if !EXIV2_TEST_VERSION(0, 18, 0)
    Exiv2::DataBuf rawData = iptcData.copy();
#else
    Exiv2::DataBuf rawData = Exiv2::IptcParser::encode(iptcData);
#endif

    if (headerType == KisMetaData::IOBackend::JpegHeader) {
        std::string header(photoshopMarker);
        header.append(1, '\0');
        header.append(photoshopBimId_, sizeof(photoshopBimId_) - 1);
        header.append(photoshopIptc_.constData(), static_cast<std::size_t>(photoshopIptc_.size()));
        header.append(2, '\0');
#if EXIV2_TEST_VERSION(0, 28, 0)
        std::int32_t size = static_cast<std::int32_t>(rawData.size());
#else
        std::int32_t size = static_cast<std::int32_t>(rawData.size_);
#endif
        const char sizeArray[] = {
            static_cast<char>((size & 0xff000000) >> 24),
            static_cast<char>((size & 0x00ff0000) >> 16),
            static_cast<char>((size & 0x0000ff00) >> 8),
            static_cast<char>(size & 0x000000ff)};
        header.append(sizeArray, sizeof(sizeArray));
        ioDevice->write(header.data(), static_cast<PkStream::pk_int64>(header.size()));
    }

#if EXIV2_TEST_VERSION(0, 28, 0)
    ioDevice->write((const char *)rawData.data(), rawData.size());
#else
    ioDevice->write((const char *)rawData.pData_, rawData.size_);
#endif
    ioDevice->close();
    return true;
}

bool KisIptcIO::canSaveAllEntries(KisMetaData::Store *store) const
{
    (void)store;
    return false;
}

bool KisIptcIO::loadFrom(KisMetaData::Store *store, PkStream *ioDevice) const
{
    initMappingsTable();
    dbgMetaData << "Loading IPTC Tags";
    ioDevice->open(PkStream::ReadOnly);
    PkByteArray arr = KisExiv2IODeviceDetail::readAllFromStream(ioDevice);
    Exiv2::IptcData iptcData;
#if !EXIV2_TEST_VERSION(0, 18, 0)
    iptcData.load((const Exiv2::byte *)arr.data(), arr.size());
#else
    Exiv2::IptcParser::decode(iptcData, (const Exiv2::byte *)arr.data(), arr.size());
#endif
    dbgMetaData << "There are" << iptcData.count() << " entries in the IPTC section";
    for (Exiv2::IptcMetadata::const_iterator it = iptcData.begin(); it != iptcData.end(); ++it) {
        dbgMetaData << "Reading info for key" << it->key().c_str();
        if (d->iptcToKMD.contains(it->key().c_str())) {
            const IPTCToKMD &iptcToKMd = d->iptcToKMD[it->key().c_str()];
            const KisMetaData::Schema *schema =
                KisMetaData::SchemaRegistry::instance()->schemaFromUri(iptcToKMd.namespaceUri);
            KisMetaData::Value value;
            if (iptcToKMd.exivTag == "Iptc.Application2.Keywords") {
                assert(it->getValue()->typeId() == Exiv2::string);
                PkString data = it->getValue()->toString().c_str();

                PkList<KisMetaData::Value> values;
                for (const PkString &entry : data.split(u',')) {
                    values.push_back(KisMetaData::Value(entry));
                }
                value = KisMetaData::Value(values, KisMetaData::Value::UnorderedArray);
            } else {
                value = exivValueToKMDValue(it->getValue(), false);
            }
            store->addEntry(KisMetaData::Entry(schema, iptcToKMd.name, value));
        }
    }
    return false;
}
