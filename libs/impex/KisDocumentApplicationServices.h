/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_DOCUMENT_APPLICATION_SERVICES_H
#define KIS_DOCUMENT_APPLICATION_SERVICES_H

#include <memory>
#include <optional>

#include <QImage>
#include <QList>
#include <QPoint>
#include <QColor>
#include <QString>
#include <QStringList>

#include <KisCumulativeUndoData.h>
#include <kis_types.h>

#include "kritaimpex_export.h"

class KoColor;
class KisDocument;
class QMutex;

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
        QString title;
        QString message;
        QStringList warnings;
        QString details;
        MessageType type = MessageType::Warning;
    };

    virtual ~KisDocumentApplicationServices();

    static KisDocumentApplicationServices *instance();
    static void setInstance(KisDocumentApplicationServices *services);

    virtual bool waitForImage(KisImageSP image, WaitMode mode);
    virtual void synchronizeDocumentViews();
    virtual void closeDocumentViews(KisDocument *document);
    virtual KoUpdaterPtr createUpdater(const QString &actionName, UpdaterMode mode);
    virtual void waitForMutexWithFeedback(QMutex &mutex, const QString &message);

    virtual RecoveryChoice chooseNamedAutosave(const QString &mainFile,
                                               const QString &autosaveFile);
    virtual void showDocumentMessage(const DocumentMessage &message);
    virtual void addRecentFile(const QString &path);
    virtual bool queryClose(KisDocument *document);

    virtual QString autoSaveLocation() const;
    virtual QImage previewCheckerboard(int tileSize) const;

    virtual bool securityBookmarksEnabled() const;
    virtual bool parentDirectoryHasPermissions(const QString &path) const;
    virtual void createSavedFileBookmark(const QString &path);

    virtual std::unique_ptr<KisDocumentBusyCursor> createBusyCursor();
    virtual std::optional<QList<KoColor>> activeColorHistory() const;

    virtual QColor defaultAssistantsColor() const;
    virtual bool backupFileEnabled() const;
    virtual int backupFileLocation() const;
    virtual int numberOfBackupFiles() const;
    virtual QString backupFileSuffix() const;
    virtual bool trimKra() const;
    virtual bool trimFramesImport() const;
    virtual int autoSaveInterval() const;
    virtual bool autoSaveFilesHidden() const;
    virtual int undoStackLimit() const;
    virtual bool useCumulativeUndoRedo() const;
    virtual KisCumulativeUndoData cumulativeUndoData() const;
    virtual bool autoPinLayersToTimeline() const;

    virtual void setDefaultGridSpacing(const QPoint &spacing);
    virtual void storeNewImageDefaults(qint32 width,
                                       qint32 height,
                                       qreal resolution,
                                       const QString &colorModel,
                                       const QString &colorDepth,
                                       const QString &colorProfile);
};

#endif // KIS_DOCUMENT_APPLICATION_SERVICES_H
