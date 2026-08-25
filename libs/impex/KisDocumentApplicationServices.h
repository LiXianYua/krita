/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_DOCUMENT_APPLICATION_SERVICES_H
#define KIS_DOCUMENT_APPLICATION_SERVICES_H

#include <memory>
#include <optional>

#include <PkColor.h>
#include <PkImage.h>
#include <PkList.h>
#include <PkPoint.h>
#include <PkString.h>
#include <PkStringList.h>

#include <KisCumulativeUndoData.h>
#include <KoCanvasResourcesInterface.h>
#include <kis_types.h>

#include "kritaimpex_export.h"

class KoColor;
class KisDocument;
class PkMutex;

class KRITAIMPEX_EXPORT KisDocumentBusyCursor
{
public:
    virtual ~KisDocumentBusyCursor();
};

/**
 * Runtime services supplied by the desktop shell to the document domain.
 *
 * The lower default implementation is deliberately headless: it never opens a
 * dialog or reaches into a view.  Krita's desktop shell installs its adapter
 * before creating documents and clears it after destroying them.
 */
class KRITAIMPEX_EXPORT KisDocumentApplicationServices
{
public:
    enum class WaitMode {
        Cancellable,
        Forced
    };

    enum class UpdaterMode {
        LoadUnthreaded,
        SaveThreaded
    };

    enum class RecoveryChoice {
        OpenAutosave,
        OpenMainFile,
        Cancel
    };

    enum class MessageType {
        Warning,
        Critical
    };

    struct DocumentMessage {
        PkString title;
        PkString message;
        PkStringList warnings;
        PkString details;
        MessageType type = MessageType::Warning;
    };

    virtual ~KisDocumentApplicationServices();

    static KisDocumentApplicationServices *instance();
    static void setInstance(KisDocumentApplicationServices *services);

    virtual bool waitForImage(KisImageSP image, WaitMode mode);
    virtual void synchronizeDocumentViews();
    virtual void closeDocumentViews(KisDocument *document);
    virtual KoCanvasResourcesInterfaceSP canvasResourcesForImage(KisImageSP image);
    virtual KoUpdaterPtr createUpdater(const PkString &actionName, UpdaterMode mode);
    virtual void waitForMutexWithFeedback(PkMutex &mutex, const PkString &message);

    virtual RecoveryChoice chooseNamedAutosave(const PkString &mainFile,
                                               const PkString &autosaveFile);
    virtual void showDocumentMessage(const DocumentMessage &message);
    virtual void addRecentFile(const PkString &path);
    virtual bool queryClose(KisDocument *document);

    virtual PkString autoSaveLocation() const;
    virtual PkImage previewCheckerboard(int tileSize) const;

    virtual bool securityBookmarksEnabled() const;
    virtual bool parentDirectoryHasPermissions(const PkString &path) const;
    virtual void createSavedFileBookmark(const PkString &path);

    virtual std::unique_ptr<KisDocumentBusyCursor> createBusyCursor();
    virtual std::optional<PkList<KoColor>> activeColorHistory() const;

    virtual PkColor defaultAssistantsColor() const;
    virtual bool backupFileEnabled() const;
    virtual int backupFileLocation() const;
    virtual int numberOfBackupFiles() const;
    virtual PkString backupFileSuffix() const;
    virtual bool trimKra() const;
    virtual bool trimFramesImport() const;
    virtual int autoSaveInterval() const;
    virtual bool autoSaveFilesHidden() const;
    virtual int undoStackLimit() const;
    virtual bool useCumulativeUndoRedo() const;
    virtual KisCumulativeUndoData cumulativeUndoData() const;
    virtual bool autoPinLayersToTimeline() const;

    virtual void setDefaultGridSpacing(const PkPoint &spacing);
    virtual void storeNewImageDefaults(qint32 width,
                                       qint32 height,
                                       qreal resolution,
                                       const PkString &colorModel,
                                       const PkString &colorDepth,
                                       const PkString &colorProfile);
};

#endif // KIS_DOCUMENT_APPLICATION_SERVICES_H
