/*
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KIS_IMPORT_EXPORT_MANAGER_H
#define KIS_IMPORT_EXPORT_MANAGER_H

#include <PkObject.h>
#include <compat/QObject>
#include <PkString.h>
#include <PkStringList.h>
#include <PkAuxTypes.h>
#include <PkSharedPointer.h>

#include "KisImportExportFilter.h"

#include "kritaimpex_export.h"
#include <functional>
#include <future>
#include <memory>

class KisDocument;
class KoProgressUpdater;
class PkWidget;

/**
 *  @brief The class managing all the filters.
 *
 *  This class manages all filters for a %Calligra application. Normally
 *  you will not have to use it, since KisMainWindow takes care of loading
 *  and saving documents.
 *
 *  @ref KisFilter
 *
 *  @author Kalle Dalheimer <kalle@kde.org>
 *  @author Torben Weis <weis@kde.org>
 *  @author Werner Trobin <trobin@kde.org>
 */
class KisImportExportBackend;
class KRITAIMPEX_EXPORT KisImportExportManager : public PkObject
{
public:
    /**
     * This enum is used to distinguish the import/export cases
     */
    enum Direction { Import = 1,  Export = 2 };

    /**
     * Create a filter manager for a document
     */
    explicit KisImportExportManager(KisDocument *document);

public:

    ~KisImportExportManager() override;

    /**
     * Imports the specified document and returns the resultant filename
     * (most likely some file in /tmp).
     * @p path can be either a URL or a filename.
     * @p documentMimeType gives importDocument a hint about what type
     * the document may be. It can be left empty.
     *
     * @return  status signals the success/error of the conversion.
     * If the returned error code is OK, then we imported the file directly
     * into the document.
     */
    KisImportExportErrorCode importDocument(const PkString &location, const PkString &mimeType);

    /**
     * @brief Exports the given file/document to the specified URL/mimetype.
     *
     * If @p mimeType is empty, then the closest matching Calligra part is searched
     * and when the method returns @p mimeType contains this mimetype.
     * Oh, well, export is a C++ keyword ;)
     */
    KisImportExportErrorCode exportDocument(const PkString &location, const PkString &realLocation, const PkByteArray &mimeType, bool showWarnings = true, KisPropertiesConfigurationSP exportConfiguration = 0, bool isAdvancedExporting = false);

    /**
     * @brief Asynchronous variant of exportDocument.
     *
     * The returned future resolves when the background export finishes. The
     * original dynamic return type was replaced by std::future (Task 8); the
     * caller polls it (KisDocument's timer) or waits on it.
     */
    std::future<KisImportExportErrorCode> exportDocumentAsync(const PkString &location, const PkString &realLocation, const PkByteArray &mimeType, KisImportExportErrorCode &status, bool showWarnings = true, KisPropertiesConfigurationSP exportConfiguration = 0, bool isAdvancedExporting = false);

    ///@name Static API
    //@{
    /**
     * Suitable for passing to KoFileDialog::setMimeTypeFilters. The default mime
     * gets set by the "users" of this method, as we do not have enough
     * information here.
     * Optionally, @p extraNativeMimeTypes are added after the native mimetype.
     */
    static PkStringList supportedMimeTypes(Direction direction);

    /**
     * @brief filterForMimeType loads the relevant import/export plugin and returns it. The caller
     * is responsible for deleting it!
     * @param mimetype the mimetype we want to import/export. If there's more than one plugin, the one
     * with the highest weight as defined in the registration will be taken
     * @param direction import or export
     * @return a pointer to the filter plugin or 0 if none could be found
     */
    static KisImportExportFilter *filterForMimeType(const PkString &mimetype, Direction direction);

    /**
     * Fill necessary information for the export filter into the properties, e.g. if the image has
     * transparency or has sRGB profile.
     */
    static void fillStaticExportConfigurationProperties(KisPropertiesConfigurationSP exportConfiguration, KisImageSP image);

    /**
     * Get if the filter manager is batch mode (true)
     * or in interactive mode (true)
     */
    bool batchMode(void) const;

    void setUpdater(KoUpdaterPtr updater);

    static PkString askForAudioFileName(const PkString &defaultDir, PkWidget *parent);

    static PkString getUriForAdditionalFile(const PkString &defaultUri, PkWidget *parent);

    /**
     * @brief Static registration entry for an import/export filter.
     *
     * The original filter discovery went through the dynamic plugin registry,
     * which D-12 removed. This static registry takes its place; it is currently
     * empty and is filled by S9 (plugin static registration). Before S9 lands,
     * supportedMimeTypes() returns an empty list and filterForMimeType() returns
     * nullptr (the export path then fails gracefully in the calling code).
     */
    struct FilterRegistration
    {
        PkStringList importMimeTypes;
        PkStringList exportMimeTypes;
        int weight = 0;
        std::function<KisImportExportFilter *()> factory;
    };
    static void registerFilter(const FilterRegistration &registration);

private:

    struct ConversionResult;
    ConversionResult convert(Direction direction, const PkString &location, const PkString &realLocation, const PkString &mimeType, bool showWarnings, KisPropertiesConfigurationSP exportConfiguration, bool isAsync, bool isAdvancedExporting = false);


    void fillStaticExportConfigurationProperties(KisPropertiesConfigurationSP exportConfiguration);
    bool askUserAboutExportConfiguration(PkSharedPointer<KisImportExportFilter> filter, KisPropertiesConfigurationSP exportConfiguration, const PkByteArray &from, const PkByteArray &to, bool batchMode, const bool showWarnings, bool *alsoAsKra, bool isAdvancedExporting = false);

    KisImportExportErrorCode doImport(const PkString &location, PkSharedPointer<KisImportExportFilter> filter);

    KisImportExportErrorCode doExport(const PkString &location, PkSharedPointer<KisImportExportFilter> filter, KisPropertiesConfigurationSP exportConfiguration, const PkString alsoAsKraLocation);
    KisImportExportErrorCode doExportImpl(const PkString &location, PkSharedPointer<KisImportExportFilter> filter, KisPropertiesConfigurationSP exportConfiguration);

    PkString getAlsoAsKraLocation(const PkString location) const;

    // Private API
    KisImportExportManager(const KisImportExportManager& rhs);
    KisImportExportManager &operator=(const KisImportExportManager& rhs);

    std::unique_ptr<KisImportExportBackend> m_backend;

    class Private;
    Private * const d;
};

#endif  // __KO_FILTER_MANAGER_H__
