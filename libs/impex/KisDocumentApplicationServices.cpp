/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDocumentApplicationServices.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QMutex>
#include <QStandardPaths>

#include <KoColor.h>
#include <KoUpdater.h>
#include <kis_image.h>

namespace
{
KisDocumentApplicationServices *s_services = nullptr;
KisDocumentApplicationServices s_headlessServices;
}

KisDocumentBusyCursor::~KisDocumentBusyCursor() = default;
KisDocumentApplicationServices::~KisDocumentApplicationServices() = default;

KisDocumentApplicationServices *KisDocumentApplicationServices::instance()
{
    return s_services ? s_services : &s_headlessServices;
}

void KisDocumentApplicationServices::setInstance(KisDocumentApplicationServices *services)
{
    s_services = services;
}

bool KisDocumentApplicationServices::waitForImage(KisImageSP image, WaitMode)
{
    if (image) {
        image->waitForDone();
    }
    return true;
}

void KisDocumentApplicationServices::synchronizeDocumentViews()
{
}

void KisDocumentApplicationServices::closeDocumentViews(KisDocument *)
{
}

KoCanvasResourcesInterfaceSP KisDocumentApplicationServices::canvasResourcesForImage(KisImageSP)
{
    return {};
}

KoUpdaterPtr KisDocumentApplicationServices::createUpdater(const QString &, UpdaterMode)
{
    return {};
}

void KisDocumentApplicationServices::waitForMutexWithFeedback(QMutex &mutex, const QString &)
{
    while (!mutex.tryLock()) {
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }
    mutex.unlock();
}

KisDocumentApplicationServices::RecoveryChoice
KisDocumentApplicationServices::chooseNamedAutosave(const QString &, const QString &)
{
    return RecoveryChoice::Cancel;
}

void KisDocumentApplicationServices::showDocumentMessage(const DocumentMessage &)
{
}

void KisDocumentApplicationServices::addRecentFile(const QString &)
{
}

bool KisDocumentApplicationServices::queryClose(KisDocument *)
{
    return true;
}

QString KisDocumentApplicationServices::autoSaveLocation() const
{
#if defined(Q_OS_WIN)
    return QDir::tempPath();
#elif defined(Q_OS_ANDROID)
    const QString path =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
            .append(QStringLiteral("/krita-backup"));
    if (!QDir(path).exists()) {
        QDir().mkpath(path);
    }
    return path;
#else
    return QDir::homePath();
#endif
}

QImage KisDocumentApplicationServices::previewCheckerboard(int tileSize) const
{
    const int size = qMax(1, tileSize);
    QImage image(size * 2, size * 2, QImage::Format_RGB32);
    image.fill(QColor(255, 255, 255));
    for (int y = 0; y < image.height(); ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (((x / size) + (y / size)) % 2) {
                line[x] = qRgb(192, 192, 192);
            }
        }
    }
    return image;
}

bool KisDocumentApplicationServices::securityBookmarksEnabled() const
{
    return false;
}

bool KisDocumentApplicationServices::parentDirectoryHasPermissions(const QString &) const
{
    return true;
}

void KisDocumentApplicationServices::createSavedFileBookmark(const QString &)
{
}

std::unique_ptr<KisDocumentBusyCursor> KisDocumentApplicationServices::createBusyCursor()
{
    return {};
}

std::optional<QList<KoColor>> KisDocumentApplicationServices::activeColorHistory() const
{
    return std::nullopt;
}

QColor KisDocumentApplicationServices::defaultAssistantsColor() const
{
    return QColor(176, 176, 176, 255);
}

bool KisDocumentApplicationServices::backupFileEnabled() const
{
    return true;
}

int KisDocumentApplicationServices::backupFileLocation() const
{
    return 0;
}

int KisDocumentApplicationServices::numberOfBackupFiles() const
{
    return 1;
}

QString KisDocumentApplicationServices::backupFileSuffix() const
{
    return QStringLiteral("~");
}

bool KisDocumentApplicationServices::trimKra() const
{
    return false;
}

bool KisDocumentApplicationServices::trimFramesImport() const
{
    return false;
}

int KisDocumentApplicationServices::autoSaveInterval() const
{
    return 7 * 60;
}

bool KisDocumentApplicationServices::autoSaveFilesHidden() const
{
    return false;
}

int KisDocumentApplicationServices::undoStackLimit() const
{
    return 200;
}

bool KisDocumentApplicationServices::useCumulativeUndoRedo() const
{
    return false;
}

KisCumulativeUndoData KisDocumentApplicationServices::cumulativeUndoData() const
{
    return KisCumulativeUndoData::defaultValue;
}

bool KisDocumentApplicationServices::autoPinLayersToTimeline() const
{
    return true;
}

void KisDocumentApplicationServices::setDefaultGridSpacing(const QPoint &)
{
}

void KisDocumentApplicationServices::storeNewImageDefaults(qint32,
                                                           qint32,
                                                           qreal,
                                                           const QString &,
                                                           const QString &,
                                                           const QString &)
{
}
