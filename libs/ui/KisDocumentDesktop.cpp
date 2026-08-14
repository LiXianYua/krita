/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDocumentDesktop.h"

#include <QApplication>
#include <QMessageBox>
#include <QMutex>
#include <QTextEdit>

#include <KoColor.h>

#include "KisApplication.h"
#include "KisAutoSaveRecoveryDialog.h"
#include "KisDocument.h"
#include "KisDocumentApplicationServices.h"
#include "KisMainWindow.h"
#include "KisPart.h"
#include "KisView.h"
#include "KisViewManager.h"
#include "KisSynchronizedConnection.h"
#include "canvas/kis_canvas2.h"
#include "kis_async_action_feedback.h"
#include "kis_canvas_resource_provider.h"
#include "kis_canvas_widget_base.h"
#include "kis_config.h"
#include "KisCursorOverrideLock.h"
#include "dialogs/KisRecoverNamedAutosaveDialog.h"

#ifdef Q_OS_MACOS
#include "KisMacosSecurityBookmarkManager.h"
#endif

namespace
{
class BusyCursor final : public KisDocumentBusyCursor
{
public:
    BusyCursor()
        : m_lock(Qt::BusyCursor)
    {
    }

private:
    KisCursorOverrideLock m_lock;
};

class DlgLoadMessages final : public QMessageBox
{
public:
    explicit DlgLoadMessages(const KisDocumentApplicationServices::DocumentMessage &message)
        : QMessageBox(message.type == KisDocumentApplicationServices::MessageType::Critical
                          ? QMessageBox::Critical
                          : QMessageBox::Warning,
                      message.title,
                      message.message,
                      QMessageBox::Ok,
                      qApp->activeWindow())
    {
        if (!message.details.isEmpty()) {
            setInformativeText(message.details);
        }
        if (!message.warnings.isEmpty()) {
            QMessageBox::setDetailedText(message.warnings.first());
            QTextEdit *messageBox = findChild<QTextEdit *>();
            if (messageBox) {
                messageBox->setAcceptRichText(true);
                QString warning = QStringLiteral("<html><body><ul>");
                for (const QString &item : message.warnings) {
                    warning += QStringLiteral("\n<li>") + item + QStringLiteral("</li>");
                }
                warning += QStringLiteral("</ul></body></html>");
                messageBox->setText(warning);
            }
        }
    }
};

class Services final : public KisDocumentApplicationServices
{
public:
    bool waitForImage(KisImageSP image, WaitMode mode) override
    {
        KisMainWindow *window = KisPart::instance()->currentMainwindow();
        if (!window || !window->viewManager()) {
            return KisDocumentApplicationServices::waitForImage(image, mode);
        }
        if (mode == WaitMode::Forced) {
            window->viewManager()->blockUntilOperationsFinishedForced(image);
            return true;
        }
        return window->viewManager()->blockUntilOperationsFinished(image);
    }

    void synchronizeDocumentViews() override
    {
        KisSynchronizedConnectionBase::forceDeliverAllSynchronizedEvents();
    }

    void closeDocumentViews(KisDocument *document) override
    {
        Q_FOREACH (KisView *view, KisPart::instance()->views()) {
            if (view->document() == document) {
                view->close();
                view->closeView();
                view->deleteLater();
            }
        }
    }

    KoCanvasResourcesInterfaceSP canvasResourcesForImage(KisImageSP image) override
    {
        KisMainWindow *window = KisPart::instance()->currentMainwindow();
        KisView *view = window ? window->activeView() : nullptr;
        return KisDocumentDesktop::canvasResourcesForImage(
            image,
            view ? view->image().toStrongRef() : KisImageSP(),
            view && view->resourceProvider() ? view->resourceProvider()->resourceManager() : nullptr);
    }

    KoUpdaterPtr createUpdater(const QString &actionName, UpdaterMode mode) override
    {
        KisMainWindow *window = KisPart::instance()->currentMainwindow();
        if (!window || !window->viewManager()) {
            return {};
        }
        return mode == UpdaterMode::SaveThreaded
            ? window->viewManager()->createThreadedUpdater(actionName)
            : window->viewManager()->createUnthreadedUpdater(actionName);
    }

    void waitForMutexWithFeedback(QMutex &mutex, const QString &message) override
    {
        KisAsyncActionFeedback feedback(message, nullptr);
        feedback.waitForMutex(mutex);
    }

    RecoveryChoice chooseNamedAutosave(const QString &mainFile,
                                       const QString &autosaveFile) override
    {
        if (auto *application = qobject_cast<KisApplication *>(qApp)) {
            application->hideSplashScreen();
        }
        KisRecoverNamedAutosaveDialog dialog(nullptr, mainFile, autosaveFile);
        dialog.exec();
        switch (dialog.result()) {
        case KisRecoverNamedAutosaveDialog::OpenAutosave:
            return RecoveryChoice::OpenAutosave;
        case KisRecoverNamedAutosaveDialog::OpenMainFile:
            return RecoveryChoice::OpenMainFile;
        default:
            return RecoveryChoice::Cancel;
        }
    }

    void showDocumentMessage(const DocumentMessage &message) override
    {
        DlgLoadMessages dialog(message);
        dialog.exec();
    }

    void addRecentFile(const QString &path) override
    {
        KisPart::instance()->addRecentURLToAllMainWindows(QUrl::fromLocalFile(path));
    }

    bool queryClose(KisDocument *document) override
    {
        for (KisView *view : KisPart::instance()->views()) {
            if (view && view->document() == document && !view->queryClose()) {
                return false;
            }
        }
        return true;
    }

    QString autoSaveLocation() const override
    {
        return KisAutoSaveRecoveryDialog::autoSaveLocation();
    }

    QImage previewCheckerboard(int tileSize) const override
    {
        return KisCanvasWidgetBase::createCheckersImage(tileSize);
    }

    bool securityBookmarksEnabled() const override
    {
#ifdef Q_OS_MACOS
        return KisMacosSecurityBookmarkManager::instance()->isSandboxed();
#else
        return false;
#endif
    }

    bool parentDirectoryHasPermissions(const QString &path) const override
    {
#ifdef Q_OS_MACOS
        return KisMacosSecurityBookmarkManager::instance()->parentDirHasPermissions(path);
#else
        Q_UNUSED(path);
        return true;
#endif
    }

    void createSavedFileBookmark(const QString &path) override
    {
#ifdef Q_OS_MACOS
        KisMacosSecurityBookmarkManager::instance()->slotCreateBookmark(path);
#else
        Q_UNUSED(path);
#endif
    }

    std::unique_ptr<KisDocumentBusyCursor> createBusyCursor() override
    {
        return std::make_unique<BusyCursor>();
    }

    std::optional<QList<KoColor>> activeColorHistory() const override
    {
        KisMainWindow *window = KisPart::instance()->currentMainwindow();
        if (!window || !window->viewManager()) {
            return std::nullopt;
        }
        return window->viewManager()->canvasResourceProvider()->colorHistory();
    }

    QColor defaultAssistantsColor() const override
    {
        return KisConfig(true).defaultAssistantsColor();
    }

    bool backupFileEnabled() const override
    {
        return KisConfig(true).backupFile();
    }

    int backupFileLocation() const override
    {
        return KisConfig(true).readEntry<int>("backupfilelocation", 0);
    }

    int numberOfBackupFiles() const override
    {
        return KisConfig(true).readEntry<int>("numberofbackupfiles", 1);
    }

    QString backupFileSuffix() const override
    {
        return KisConfig(true).readEntry<QString>("backupfilesuffix", QStringLiteral("~"));
    }

    bool trimKra() const override
    {
        return KisConfig(true).trimKra();
    }

    bool trimFramesImport() const override
    {
        return KisConfig(true).trimFramesImport();
    }

    int autoSaveInterval() const override
    {
        return KisConfig(true).autoSaveInterval();
    }

    bool autoSaveFilesHidden() const override
    {
        return KisConfig(true).readEntry<bool>("autosavefileshidden");
    }

    int undoStackLimit() const override
    {
        return KisConfig(true).undoStackLimit();
    }

    bool useCumulativeUndoRedo() const override
    {
        return KisConfig(true).useCumulativeUndoRedo();
    }

    KisCumulativeUndoData cumulativeUndoData() const override
    {
        return KisConfig(true).cumulativeUndoData();
    }

    bool autoPinLayersToTimeline() const override
    {
        return KisConfig(true).autoPinLayersToTimeline();
    }

    void setDefaultGridSpacing(const QPoint &spacing) override
    {
        KisConfig(false).setDefaultGridSpacing(spacing);
    }

    void storeNewImageDefaults(qint32 width,
                               qint32 height,
                               qreal resolution,
                               const QString &colorModel,
                               const QString &colorDepth,
                               const QString &colorProfile) override
    {
        KisConfig config(false);
        config.defImageWidth(width);
        config.defImageHeight(height);
        config.defImageResolution(resolution);
        if (!config.useDefaultColorSpace()) {
            config.defColorModel(colorModel);
            config.setDefaultColorDepth(colorDepth);
            config.defColorProfile(colorProfile);
        }
    }
};

Services s_services;
}

KoCanvasResourcesInterfaceSP KisDocumentDesktop::canvasResourcesForImage(
    KisImageSP requestedImage,
    KisImageSP activeImage,
    KoCanvasResourceProvider *resourceManager)
{
    if (!requestedImage || requestedImage.data() != activeImage.data() || !resourceManager) {
        return {};
    }
    return resourceManager->canvasResourcesInterface();
}

void initializeKisDocumentDesktopServices()
{
    KisDocumentApplicationServices::setInstance(&s_services);
}

void clearKisDocumentDesktopServices()
{
    KisDocumentApplicationServices::setInstance(nullptr);
}
