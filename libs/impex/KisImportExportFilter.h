/*
    This file is part of the Calligra libraries

    SPDX-FileCopyrightText: 2001 Werner Trobin <trobin@kde.org>
    SPDX-FileCopyrightText: 2002 Werner Trobin <trobin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIS_IMPORT_EXPORT_FILTER_H
#define KIS_IMPORT_EXPORT_FILTER_H

#include <PkObject.h>
#include <PkStream.h>
#include <PkMap.h>
#include <PkPointer.h>
#include <PkString.h>
#include <PkList.h>
#include <PkAuxTypes.h>
#include <PkStringList.h>
#include <utility>
#include <KoID.h>
#include <kis_properties_configuration.h>
#include <kis_types.h>
#include <KisExportCheckBase.h>

class KoUpdater;
class KisDocument;
class KisImportUserFeedbackInterface;

#include "kritaimpex_export.h"
#include "KisImportExportErrorCode.h"

/**
 * @brief The base class for import and export filters.
 *
 * Derive your filter class from this base class and implement
 * the @ref convert() method. Don't forget to specify the Q_OBJECT
 * macro in your class even if you don't use signals or slots.
 * This is needed as filters are created on the fly.
 *
 * @note Take care: The m_chain pointer is invalid while the constructor
 * runs due to the implementation -- @em don't use it in the constructor.
 * After the constructor, when running the @ref convert() method it's
 * guaranteed to be valid, so no need to check against 0.
 *
 * @note If the code is compiled in debug mode, setting CALLIGRA_DEBUG_FILTERS
 * environment variable to any value disables deletion of temporary files while
 * importing/exporting. This is useful for testing purposes.
 *
 * @author Werner Trobin <trobin@kde.org>
 * @todo the class has no constructor and therefore cannot initialize its private class
 */
class KRITAIMPEX_EXPORT KisImportExportFilter : public PkObject
{
    Q_OBJECT
public:
    static const PkString ImageContainsTransparencyTag;
    static const PkString ColorModelIDTag;
    static const PkString ColorDepthIDTag;
    static const PkString sRGBTag;
    static const PkString HDRTag;
    static const PkString CICPPrimariesTag;
    static const PkString CICPTransferCharacteristicsTag;
public:

    ~KisImportExportFilter() override;

    void setBatchMode(bool batchmode);
    void setImportUserFeedBackInterface(KisImportUserFeedbackInterface *interface);
    void setFilename(const PkString &filename);
    void setRealFilename(const PkString &filename);
    void setMimeType(const PkString &mime);
    void setUpdater(PkPointer<KoUpdater> updater);
    PkPointer<KoUpdater> updater();

    /**
     * The filter chain calls this method to perform the actual conversion.
     * The passed mimetypes should be a pair of those you specified in your
     * .desktop file.
     * You @em have to implement this method to make the filter work.
     *
     * @return The error status, see the @ref #ConversionStatus enum.
     *         KisImportExportFilter::OK means that everything is alright.
     */
    virtual KisImportExportErrorCode convert(KisDocument *document, PkStream *io, KisPropertiesConfigurationSP configuration = 0) = 0;

    /**
     * @brief defaultConfiguration defines the default settings for the given import export filter
     * @param from The mimetype of the source file/document
     * @param to The mimetype of the destination file/document
     * @return a serializable KisPropertiesConfiguration object
     */
    virtual KisPropertiesConfigurationSP defaultConfiguration(const PkByteArray& from = PkByteArray(), const PkByteArray& to = PkByteArray()) const;

    /**
     * @brief lastSavedConfiguration return the last saved configuration for this filter
     * @param from The mimetype of the source file/document
     * @param to The mimetype of the destination file/document
     * @return a serializable KisPropertiesConfiguration object
     */
    KisPropertiesConfigurationSP lastSavedConfiguration(const PkByteArray &from = PkByteArray(), const PkByteArray &to = PkByteArray()) const;

    /**
     * @brief generate and return the list of capabilities of this export filter. The list
     * @return returns the list of capabilities of this export filter
     */
    virtual PkMap<PkString, KisExportCheckBase*> exportChecks();

    /**
     * @brief exportSupportsGuides
     * Because guides are in the document and not the image,
     * checking for guides cannot be made an exportCheck.
     * @return whether this filter supports exporting guides
     */
    virtual bool exportSupportsGuides() const;

    /// Override and return false for the filters that use a library that cannot handle file handles, only file names.
    virtual bool supportsIO() const { return true; }

    /// Verify whether the given file is correct and readable
    virtual PkString verify(const PkString &fileName) const;

protected:
    /**
     * This is the constructor your filter has to call, obviously.
     */
    KisImportExportFilter(PkObject *parent = 0);

    PkString filename() const;
    PkString realFilename() const;
    bool batchMode() const;
    KisImportUserFeedbackInterface* importUserFeedBackInterface() const;
    PkByteArray mimeType() const;

    void setProgress(int value);
    virtual void initializeCapabilities();
    void addCapability(KisExportCheckBase *capability);
    void addSupportedColorModels(PkList<std::pair<KoID, KoID> > supportedColorModels, const PkString &name, KisExportCheckBase::Level level = KisExportCheckBase::PARTIALLY);

    PkString verifyZiPBasedFiles(const PkString &fileName, const PkStringList &filesToCheck) const;

private:

    KisImportExportFilter(const KisImportExportFilter& rhs);
    KisImportExportFilter& operator=(const KisImportExportFilter& rhs);

    class Private;
    Private *const d;

};

#endif
