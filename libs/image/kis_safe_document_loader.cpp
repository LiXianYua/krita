/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_safe_document_loader.cpp 阻塞登记（S-06 Task 8 批次C2）
// 
// 本文件不进薄壳，保留 Qt 原样。阻塞原因：
//   * QFileSystemWatcher/QTimer::singleShot/QCoreApplication::processEvents/
//     QRandomGenerator/QDateTime/QDir/QTemporaryFile 全量未剥；
//   * FileSystemWatcherWrapper 的 Qt 信号槽（fileChanged/fileExistsStateChanged）
//     与 QObject 生命周期绑定，且文本流输出走 QTextStream。
// ===========================================================================


#include <QtCore/QDebug>
#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>

#include "kis_safe_document_loader.h"

#include <utility>

#include <QTimer>
#include <QFileSystemWatcher>
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>

#include <KoStore.h>
#include <QTemporaryFile>

#include "kis_signal_compressor.h"
#include "KisUsageLogger.h"

#include <kis_global.h>

namespace {

PkString toPkString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}

KisSafeDocumentLoader::ImageLoader &defaultImageLoader()
{
    static KisSafeDocumentLoader::ImageLoader loader;
    return loader;
}

}

class FileSystemWatcherWrapper : public QObject
{
    Q_OBJECT

private:
    enum FileState {
        Exists = 0,
        Reattaching,
        Lost
    };

    struct FileEntry
    {
        int numConnections = 0;
        QElapsedTimer lostTimer;
        FileState state = Exists;
    };

public:
    FileSystemWatcherWrapper()
        : m_reattachmentCompressor(100, KisSignalCompressor::FIRST_INACTIVE),
          m_lostCompressor(1000, KisSignalCompressor::FIRST_INACTIVE)

    {
        connect(&m_watcher, SIGNAL(fileChanged(QString)), SLOT(slotFileChanged(QString)));
        m_reattachmentConnection =
            PkObject::connect(&m_reattachmentCompressor, &KisSignalCompressor::timeout,
                              &m_reattachmentCompressor,
                              [this]() { slotReattachFiles(); });
        m_lostConnection =
            PkObject::connect(&m_lostCompressor, &KisSignalCompressor::timeout,
                              &m_lostCompressor,
                              [this]() { slotFindLostFiles(); });
    }

    ~FileSystemWatcherWrapper() override
    {
        PkObject::disconnect(m_lostConnection);
        PkObject::disconnect(m_reattachmentConnection);
    }

    bool addPath(const QString &file) {
        bool result = true;
        const QString ufile = unifyFilePath(file);

        if (m_fileEntries.contains(ufile)) {
            m_fileEntries[ufile].numConnections++;
        } else {
            m_fileEntries.insert(ufile, {1, {}, Exists});
            result = m_watcher.addPath(ufile);
        }

        return result;
    }

    bool removePath(const QString &file) {
        bool result = true;
        const QString ufile = unifyFilePath(file);

        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(m_fileEntries.contains(ufile), false);

        if (m_fileEntries[ufile].numConnections == 1) {
            m_fileEntries.remove(ufile);
            result = m_watcher.removePath(ufile);
        } else {
            m_fileEntries[ufile].numConnections--;
        }
        return result;
    }

    QStringList files() const {
        return m_watcher.files();
    }

private Q_SLOTS:
    void slotFileChanged(const QString &path) {

        KIS_SAFE_ASSERT_RECOVER_RETURN(m_fileEntries.contains(path));

        FileEntry &entry = m_fileEntries[path];

        // re-add the file after QSaveFile optimization
        if (!m_watcher.files().contains(path)) {

            if (QFileInfo(path).exists()) {
                const FileState oldState = entry.state;

                m_watcher.addPath(path);
                entry.state = Exists;

                if (oldState == Lost) {
                    Q_EMIT fileExistsStateChanged(path, true);
                } else {
                    Q_EMIT fileChanged(path);
                }

            } else {

                if (entry.state == Exists) {
                    entry.state = Reattaching;
                    entry.lostTimer.start();
                    m_reattachmentCompressor.start();

                } else if (entry.state == Reattaching) {
                    if (entry.lostTimer.elapsed() > 10000) {
                        entry.state = Lost;
                        m_lostCompressor.start();
                        Q_EMIT fileExistsStateChanged(path, false);
                    } else {
                        m_reattachmentCompressor.start();
                    }

                } else if (entry.state == Lost) {
                    m_lostCompressor.start();
                }


#if 0
                const bool shouldSpitWarning =
                    absenceTimeMSec <= 600000 &&
                        ((absenceTimeMSec >= 60000 && (absenceTimeMSec % 60000 == 0)) ||
                         (absenceTimeMSec >= 10000 && (absenceTimeMSec % 10000 == 0)));

                if (shouldSpitWarning) {
                    QString message;
                    QTextStream log(&message);
                    KisPortingUtils::setUtf8OnStream(log);

                    log << "WARNING: couldn't reconnect to a removed file layer's file (" << path << "). File is not available for " << absenceTimeMSec / 1000 << " seconds";

                    qWarning() << message;
                    KisUsageLogger::log(message);

                    if (absenceTimeMSec == 600000) {
                        message.clear();
                        log.reset();

                        log << "Giving up... :( No more reports about " << path;

                        qWarning() << message;
                        KisUsageLogger::log(message);
                    }
                }
#endif
            }
        } else {
            Q_EMIT fileChanged(path);
        }
    }

    void slotFindLostFiles() {
        for (auto it = m_fileEntries.constBegin(); it != m_fileEntries.constEnd(); ++it) {
            if (it.value().state == Lost)
            slotFileChanged(it.key());
        }
    }

    void slotReattachFiles() {
        for (auto it = m_fileEntries.constBegin(); it != m_fileEntries.constEnd(); ++it) {
            if (it.value().state == Reattaching)
            slotFileChanged(it.key());
        }
    }


Q_SIGNALS:
    void fileChanged(const QString &path);
    void fileExistsStateChanged(const QString &path, bool exists);

public:
    static QString unifyFilePath(const QString &path) {
        return QFileInfo(path).absoluteFilePath();
    }

private:
    QFileSystemWatcher m_watcher;
    QHash<QString, int> m_pathCount;
    KisSignalCompressor m_reattachmentCompressor;
    KisSignalCompressor m_lostCompressor;
    PkConnection m_reattachmentConnection;
    PkConnection m_lostConnection;
    QHash<QString, int> m_lostFilesAbsenceCounter;
    QHash<QString, FileEntry> m_fileEntries;
};

Q_GLOBAL_STATIC(FileSystemWatcherWrapper, s_fileSystemWatcher)


struct KisSafeDocumentLoader::Private
{
    explicit Private(ImageLoader loader)
        : fileChangedSignalCompressor(500 /* ms */, KisSignalCompressor::POSTPONE)
        , imageLoader(std::move(loader))
    {
    }

    KisSignalCompressor fileChangedSignalCompressor;
    PkConnection fileChangedConnection;
    ImageLoader imageLoader;
    bool isLoading = false;
    bool fileChangedFlag = false;
    QString path;
    QString temporaryPath;

    qint64 initialFileSize {0};
    QDateTime initialFileTimeStamp;

    int failureCount {0};
};

KisSafeDocumentLoader::KisSafeDocumentLoader(const QString &path, QObject *parent)
    : KisSafeDocumentLoader(path, {}, parent)
{
}

KisSafeDocumentLoader::KisSafeDocumentLoader(const QString &path,
                                             ImageLoader imageLoader,
                                             QObject *parent)
    : QObject(parent),
      m_d(new Private(std::move(imageLoader)))
{
    connect(s_fileSystemWatcher, SIGNAL(fileChanged(QString)),
            SLOT(fileChanged(QString)));

    connect(s_fileSystemWatcher, SIGNAL(fileExistsStateChanged(QString, bool)),
            SLOT(slotFileExistsStateChanged(QString, bool)));

    m_d->fileChangedConnection =
        PkObject::connect(&m_d->fileChangedSignalCompressor,
                          &KisSignalCompressor::timeout,
                          &m_d->fileChangedSignalCompressor,
                          [this]() { fileChangedCompressed(); });

    setPath(path);
}

void KisSafeDocumentLoader::setDefaultImageLoader(ImageLoader imageLoader)
{
    defaultImageLoader() = std::move(imageLoader);
}

KisSafeDocumentLoader::~KisSafeDocumentLoader()
{
    PkObject::disconnect(m_d->fileChangedConnection);

    if (!m_d->path.isEmpty()) {
        s_fileSystemWatcher->removePath(m_d->path);
    }

    delete m_d;
}

void KisSafeDocumentLoader::setPath(const QString &path)
{
    if (path.isEmpty()) return;

    if (!m_d->path.isEmpty()) {
        s_fileSystemWatcher->removePath(m_d->path);
    }

    m_d->path = path;
    s_fileSystemWatcher->addPath(m_d->path);
}

void KisSafeDocumentLoader::reloadImage()
{
    fileChangedCompressed(true);
}

void KisSafeDocumentLoader::fileChanged(QString path)
{
    if (FileSystemWatcherWrapper::unifyFilePath(m_d->path) == path) {
        m_d->fileChangedFlag = true;
        m_d->fileChangedSignalCompressor.start();
    }
}

void KisSafeDocumentLoader::slotFileExistsStateChanged(const QString &path, bool fileExists)
{
    if (FileSystemWatcherWrapper::unifyFilePath(m_d->path) == path) {
        Q_EMIT fileExistsStateChanged(fileExists);
        if (fileExists) {
            fileChanged(path);
        }
    }
}

void KisSafeDocumentLoader::fileChangedCompressed(bool sync)
{
    if (m_d->isLoading) return;

    QFileInfo initialFileInfo(m_d->path);
    m_d->initialFileSize = initialFileInfo.size();
    m_d->initialFileTimeStamp = initialFileInfo.lastModified();

    // it may happen when the file is flushed by
    // so other application
    if (!m_d->initialFileSize) return;

    m_d->isLoading = true;
    m_d->fileChangedFlag = false;

    m_d->temporaryPath =
            QDir::tempPath() + '/' +
            QString("krita_file_layer_copy_%1_%2.%3")
            .arg(QCoreApplication::applicationPid())
            .arg(QRandomGenerator::global()->generate())
            .arg(initialFileInfo.suffix());

    QFile::copy(m_d->path, m_d->temporaryPath);


    if (!sync) {
        QTimer::singleShot(100, Qt::CoarseTimer, this, SLOT(delayedLoadStart()));
    } else {
        QCoreApplication::processEvents();
        delayedLoadStart();
    }
}

void KisSafeDocumentLoader::delayedLoadStart()
{
    QFileInfo originalInfo(m_d->path);
    QFileInfo tempInfo(m_d->temporaryPath);
    bool successfullyLoaded = false;
    LoadResult loadResult;

    if (!m_d->fileChangedFlag &&
            originalInfo.size() == m_d->initialFileSize &&
            originalInfo.lastModified() == m_d->initialFileTimeStamp &&
            tempInfo.size() == m_d->initialFileSize) {

        const ImageLoader imageLoader = m_d->imageLoader ? m_d->imageLoader : defaultImageLoader();

        auto loadPathNatively = [&imageLoader, &loadResult](const QString &path) -> bool {
            if (!imageLoader) {
                return false;
            }

            loadResult = imageLoader(path);
            return bool(loadResult);
        };

        if (m_d->path.toLower().endsWith("ora") || m_d->path.toLower().endsWith("kra")) {
            QScopedPointer<KoStore> store(KoStore::createStore(toPkString(m_d->temporaryPath), KoStore::Read));
            if (store && !store->bad()) {
                if (store->open(toPkString(QStringLiteral("mergedimage.png")))) {
                    /**
                     * TODO: Ideally the configured image loader should allow
                     * loading from a QIODevice, but currently its contract
                     * accepts a local file path only. That is why
                     * we just extract the PNG into a temporary file and
                     * load it separately.
                     *
                     * NOTE: we cannot use QImage for loading, since it strips
                     * the color profile attached to the PNG file
                     */
                    qint64 totalWritten = 0;
                    const qint64 expectedFileSize = store->size();
                    QTemporaryFile temporaryFile(QDir::tempPath() + QLatin1String("/krita_merged_image_XXXXXX.png"));
                    if (temporaryFile.open()) {
                        QByteArray buffer(BUFSIZ, 0);

                        while (true) {
                            qint64 read = store->read(buffer.data(), buffer.size());
                            if (read < 0) {
                                warnKrita << "Failed to read from mergedimage.png for the file layer's projection";
                                break;
                            } else if (read == 0) {
                                // End of file
                                break;
                            } else {
                                // Successful read, try to write it.
                                qint64 written = temporaryFile.write(buffer.constData(), read);
                                if (written < 0) {
                                    // Write error.
                                    warnKrita << "Failed to write mergedimage.png into a temporary file for the file layer's projection"
                                              << temporaryFile.fileName() << ":" << temporaryFile.errorString();
                                    break;
                                }
                                // We may not have written as much as we read, but we handle
                                // that at the end.
                                totalWritten += written;
                            }
                        }

                        temporaryFile.close();
                    } else {
                        warnKrita << "Failed to open temporary file for mergedimage.png for the file layer's projection"
                                  << temporaryFile.fileName() << ":" << temporaryFile.errorString();
                    }
                    store->close();

                    if (totalWritten == expectedFileSize) {
                        successfullyLoaded = loadPathNatively(temporaryFile.fileName());
                    } else {
                        successfullyLoaded = false;
                    }
                }
                else {
                    qWarning() << "delayedLoadStart: Could not open mergedimage.png";
                }
            }
            else {
                qWarning() << "delayedLoadStart: Store was bad";
            }
        }
        else {
            successfullyLoaded = loadPathNatively(m_d->temporaryPath);
        }
    } else {
        dbgKrita << "File was modified externally. Restarting.";
        dbgKrita << ppVar(m_d->fileChangedFlag);
        dbgKrita << ppVar(m_d->initialFileSize);
        dbgKrita << ppVar(m_d->initialFileTimeStamp);
        dbgKrita << ppVar(originalInfo.size());
        dbgKrita << ppVar(originalInfo.lastModified());
        dbgKrita << ppVar(tempInfo.size());
    }

    QFile::remove(m_d->temporaryPath);
    m_d->isLoading = false;

    if (!successfullyLoaded) {
        // Restart the attempt
        m_d->failureCount++;
        if (m_d->failureCount >= 3) {
            Q_EMIT loadingFailed();
        }
        else {
            m_d->fileChangedSignalCompressor.start();
        }
    }
    else {
        Q_EMIT loadingFinished(loadResult.paintDevice,
                              loadResult.xRes,
                              loadResult.yRes,
                              loadResult.size);
    }
}

#include "kis_safe_document_loader.moc"
