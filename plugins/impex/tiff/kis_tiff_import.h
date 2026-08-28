/*
 *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _KIS_TIFF_IMPORT_H_
#define _KIS_TIFF_IMPORT_H_

#include <PkVariant.h>

#include <tiffio.h>

#include <KisImportExportFilter.h>
#include <config-tiff.h>
#include <config-jpeg.h>
#include <kis_types.h>

class PkMemoryStream;
class KisTiffPsdLayerRecord;
class KisTiffPsdResourceRecord;
struct KisTiffBasicInfo;

class KisTIFFImport : public KisImportExportFilter
{
public:
    KisTIFFImport(PkObject *parent, const PkVariantList &);
    ~KisTIFFImport() override;
    bool supportsIO() const override { return true; }
    KisImportExportErrorCode
    convert(KisDocument *document,
            PkStream *io,
            KisPropertiesConfigurationSP configuration = nullptr) override;

private:
    KisImportExportErrorCode readTIFFDirectory(KisDocument *m_doc, TIFF *image);

    /**
     * Imports the image from the TIFF descriptor.
     */
    KisImportExportErrorCode readImageFromTiff(KisDocument *m_doc,
                                               TIFF *image,
                                               KisTiffBasicInfo &basicInfo);

    /**
     * Imports the image from the PSD descriptor.
     */
    KisImportExportErrorCode readImageFromPsd(KisDocument *m_doc,
                                              TIFF *image,
                                              KisTiffBasicInfo &basicInfo);

#ifdef TIFF_HAS_PSD_TAGS
    /**
     * Imports the image from the PSD descriptor attached.
     * If this function is invoked, readTIFFDirectory will only
     * parse the first image descriptor.
     */
    KisImportExportErrorCode
    readImageFromPsdRecords(KisDocument *m_doc,
                     const KisTiffPsdLayerRecord &photoshopLayerRecord,
                     KisTiffPsdResourceRecord &photoshopImageResourceRecord,
                     PkMemoryStream &photoshopLayerData,
                     const KisTiffBasicInfo &basicInfo);
#endif // TIFF_HAS_PSD_TAGS

    KisImageSP m_image;
    bool m_photoshopBlockParsed = false;

    TIFFErrorHandler oldErrHandler = nullptr;
    TIFFErrorHandler oldWarnHandler = nullptr;
};

#endif
