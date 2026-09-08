/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_safe_document_loader.h"

#include <KoStore.h>

#include <PkEventLoop.h>
#include <PkThreadCallQueue.h>
#include <PkTimer.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using namespace std::chrono_literals;

KisSafeDocumentLoader::ImageLoader &defaultImageLoader()
{
    static KisSafeDocumentLoader::ImageLoader loader;
    return loader;
}

fs::path nativePath(const PkString &path)
{
    return fs::u8path(path.PkToUtf8());
}

PkString portablePath(const fs::path &path)
{
    return PkString(path.u8string().c_str());
}

struct FileSnapshot
{
    bool exists = false;
    std::uintmax_t size = 0;
    fs::file_time_type modified {};
};

FileSnapshot snapshot(const PkString &path)
{
    FileSnapshot result;
    std::error_code error;
    const fs::path native = nativePath(path);
    result.exists = fs::exists(native, error) && !error;
    if (!result.exists) return result;

    result.size = fs::file_size(native, error);
    if (error) {
        result.exists = false;
        result.size = 0;
        return result;
    }

    result.modified = fs::last_write_time(native, error);
    if (error) {
        result.exists = false;
        result.size = 0;
    }
    return result;
}

PkString uniqueTemporaryPath(const char *prefix, const std::string &suffix)
{
    static std::atomic<std::uint64_t> nextId {0};
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string name = std::string(prefix) + std::to_string(tick) + "_" +
        std::to_string(nextId.fetch_add(1, std::memory_order_relaxed)) + suffix;
    std::error_code error;
    fs::path directory = fs::temp_directory_path(error);
    if (error) directory = fs::current_path(error);
    return portablePath(directory / name);
}

class FileSystemWatcherWrapper final : public PkObject
{
public:
    FileSystemWatcherWrapper()
        : m_targetThread(PkThreadCallQueue::warmUpCurrentThread())
        , m_worker([this] { run(); })
    {
    }

    ~FileSystemWatcherWrapper() override
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_wake.notify_all();
        if (m_worker.joinable()) m_worker.join();
    }

    bool addPath(const PkString &file)
    {
        const PkString unified = unifyFilePath(file);
        const FileSnapshot current = snapshot(unified);
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_entries.find(unified);
        if (it != m_entries.end()) {
            ++it->second.numConnections;
            return current.exists;
        }

        Entry entry;
        entry.numConnections = 1;
        entry.last = current;
        if (!current.exists) entry.missingSince = std::chrono::steady_clock::now();
        m_entries.emplace(unified, entry);
        m_wake.notify_all();
        return current.exists;
    }

    bool removePath(const PkString &file)
    {
        const PkString unified = unifyFilePath(file);
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_entries.find(unified);
        if (it == m_entries.end()) return false;
        if (--it->second.numConnections == 0) m_entries.erase(it);
        return true;
    }

    static PkString unifyFilePath(const PkString &path)
    {
        std::error_code error;
        fs::path absolute = fs::absolute(nativePath(path), error);
        if (error) absolute = nativePath(path);
        return portablePath(absolute.lexically_normal());
    }

    void fileChanged(PkString path)
    {
        activateSignal(this,
                       PkMemberFnKey::from(&FileSystemWatcherWrapper::fileChanged),
                       path);
    }

    void fileExistsStateChanged(PkString path, bool exists)
    {
        activateSignal(this,
                       PkMemberFnKey::from(&FileSystemWatcherWrapper::fileExistsStateChanged),
                       path,
                       exists);
    }

private:
    struct Entry
    {
        int numConnections = 0;
        FileSnapshot last;
        std::chrono::steady_clock::time_point missingSince {};
        bool lossReported = false;
    };

    enum class EventType {
        Changed,
        ExistsState
    };

    struct Event
    {
        EventType type;
        PkString path;
        bool exists = false;
    };

    void run()
    {
        while (true) {
            std::vector<Event> events;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_wake.wait_for(lock, 50ms, [this] { return m_stopping; });
                if (m_stopping) return;

                const auto now = std::chrono::steady_clock::now();
                for (auto &item : m_entries) {
                    const PkString &path = item.first;
                    Entry &entry = item.second;
                    const FileSnapshot current = snapshot(path);

                    if (entry.last.exists && !current.exists) {
                        entry.last = current;
                        entry.missingSince = now;
                        entry.lossReported = false;
                        continue;
                    }

                    if (!entry.last.exists && !current.exists) {
                        if (!entry.lossReported && entry.missingSince.time_since_epoch().count() &&
                            now - entry.missingSince > 10s) {
                            entry.lossReported = true;
                            events.push_back({EventType::ExistsState, path, false});
                        }
                        continue;
                    }

                    if (!entry.last.exists && current.exists) {
                        const bool wasReportedLost = entry.lossReported;
                        entry.last = current;
                        entry.lossReported = false;
                        events.push_back({wasReportedLost ? EventType::ExistsState : EventType::Changed,
                                          path,
                                          true});
                        continue;
                    }

                    if (current.size != entry.last.size || current.modified != entry.last.modified) {
                        entry.last = current;
                        events.push_back({EventType::Changed, path, true});
                    }
                }
            }

            for (const Event &event : events) {
                PkThreadCallQueue::post(
                    m_targetThread,
                    [this, event] {
                        if (event.type == EventType::Changed) {
                            fileChanged(event.path);
                        } else {
                            fileExistsStateChanged(event.path, event.exists);
                        }
                    },
                    callLifetime());
            }
        }
    }

    const PkThreadId m_targetThread;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    std::map<PkString, Entry> m_entries;
    bool m_stopping = false;
    std::thread m_worker;
};

FileSystemWatcherWrapper &fileSystemWatcher()
{
    static FileSystemWatcherWrapper watcher;
    return watcher;
}

} // namespace

struct KisSafeDocumentLoader::Private
{
    explicit Private(ImageLoader loader)
        : imageLoader(std::move(loader))
    {
    }

    PkTimer fileChangedTimer;
    PkTimer delayedLoadTimer;
    PkConnection fileChangedConnection;
    PkConnection fileExistsConnection;
    ImageLoader imageLoader;
    bool isLoading = false;
    bool fileChangedFlag = false;
    PkString path;
    PkString temporaryPath;
    std::uintmax_t initialFileSize = 0;
    fs::file_time_type initialFileTimeStamp {};
    int failureCount = 0;
};

KisSafeDocumentLoader::KisSafeDocumentLoader(const PkString &path, PkObject *parent)
    : KisSafeDocumentLoader(path, {}, parent)
{
}

KisSafeDocumentLoader::KisSafeDocumentLoader(const PkString &path,
                                             ImageLoader imageLoader,
                                             PkObject *parent)
    : PkObject(parent)
    , m_d(new Private(std::move(imageLoader)))
{
    m_d->fileChangedConnection =
        PkObject::connect(&fileSystemWatcher(),
                          &FileSystemWatcherWrapper::fileChanged,
                          this,
                          [this](PkString changedPath) { fileChanged(std::move(changedPath)); });
    m_d->fileExistsConnection =
        PkObject::connect(&fileSystemWatcher(),
                          &FileSystemWatcherWrapper::fileExistsStateChanged,
                          this,
                          [this](PkString changedPath, bool exists) {
                              slotFileExistsStateChanged(std::move(changedPath), exists);
                          });
    setPath(path);
}

void KisSafeDocumentLoader::setDefaultImageLoader(ImageLoader imageLoader)
{
    defaultImageLoader() = std::move(imageLoader);
}

KisSafeDocumentLoader::~KisSafeDocumentLoader()
{
    m_d->fileChangedTimer.stop();
    m_d->delayedLoadTimer.stop();
    PkObject::disconnect(m_d->fileChangedConnection);
    PkObject::disconnect(m_d->fileExistsConnection);
    if (!m_d->path.isEmpty()) fileSystemWatcher().removePath(m_d->path);

    std::error_code error;
    if (!m_d->temporaryPath.isEmpty()) fs::remove(nativePath(m_d->temporaryPath), error);
    delete m_d;
}

void KisSafeDocumentLoader::setPath(const PkString &path)
{
    if (path.isEmpty()) return;
    if (!m_d->path.isEmpty()) fileSystemWatcher().removePath(m_d->path);
    m_d->path = path;
    fileSystemWatcher().addPath(m_d->path);
}

void KisSafeDocumentLoader::reloadImage()
{
    fileChangedCompressed(true);
}

void KisSafeDocumentLoader::fileChanged(PkString path)
{
    if (FileSystemWatcherWrapper::unifyFilePath(m_d->path) != path) return;
    m_d->fileChangedFlag = true;
    m_d->fileChangedTimer.start(500ms, [this] { fileChangedCompressed(); }, true);
}

void KisSafeDocumentLoader::slotFileExistsStateChanged(PkString path, bool fileExists)
{
    if (FileSystemWatcherWrapper::unifyFilePath(m_d->path) != path) return;
    fileExistsStateChanged(fileExists);
    if (fileExists) fileChanged(std::move(path));
}

void KisSafeDocumentLoader::fileChangedCompressed(bool sync)
{
    if (m_d->isLoading) return;

    const FileSnapshot initial = snapshot(m_d->path);
    m_d->initialFileSize = initial.size;
    m_d->initialFileTimeStamp = initial.modified;
    if (!initial.exists || !m_d->initialFileSize) return;

    m_d->isLoading = true;
    m_d->fileChangedFlag = false;
    const std::string suffix = nativePath(m_d->path).extension().u8string();
    m_d->temporaryPath = uniqueTemporaryPath("krita_file_layer_copy_", suffix);

    std::error_code error;
    fs::copy_file(nativePath(m_d->path),
                  nativePath(m_d->temporaryPath),
                  fs::copy_options::overwrite_existing,
                  error);

    if (sync) {
        PkEventLoop::processEvents();
        delayedLoadStart();
    } else {
        m_d->delayedLoadTimer.start(100ms, [this] { delayedLoadStart(); }, true);
    }
}

void KisSafeDocumentLoader::delayedLoadStart()
{
    const FileSnapshot original = snapshot(m_d->path);
    const FileSnapshot temporary = snapshot(m_d->temporaryPath);
    bool successfullyLoaded = false;
    LoadResult loadResult;

    if (!m_d->fileChangedFlag &&
        original.exists &&
        original.size == m_d->initialFileSize &&
        original.modified == m_d->initialFileTimeStamp &&
        temporary.exists &&
        temporary.size == m_d->initialFileSize) {

        const ImageLoader imageLoader = m_d->imageLoader ? m_d->imageLoader : defaultImageLoader();
        auto loadPathNatively = [&imageLoader, &loadResult](const PkString &path) {
            if (!imageLoader) return false;
            loadResult = imageLoader(path);
            return bool(loadResult);
        };

        const PkString lowerPath = m_d->path.toLower();
        if (lowerPath.endsWith("ora") || lowerPath.endsWith("kra")) {
            std::unique_ptr<KoStore> store(KoStore::createStore(m_d->temporaryPath, KoStore::Read));
            if (store && !store->bad() && store->open("mergedimage.png")) {
                const std::int64_t expectedSize = store->size();
                const PkString mergedPath = uniqueTemporaryPath("krita_merged_image_", ".png");
                std::ofstream output(nativePath(mergedPath), std::ios::binary | std::ios::trunc);
                std::int64_t totalWritten = 0;
                std::array<char, BUFSIZ> buffer {};

                while (output) {
                    const std::int64_t bytesRead = store->read(buffer.data(), buffer.size());
                    if (bytesRead <= 0) break;
                    output.write(buffer.data(), bytesRead);
                    if (output) totalWritten += bytesRead;
                }
                output.close();
                store->close();

                if (totalWritten == expectedSize) {
                    successfullyLoaded = loadPathNatively(mergedPath);
                }
                std::error_code error;
                fs::remove(nativePath(mergedPath), error);
            }
        } else {
            successfullyLoaded = loadPathNatively(m_d->temporaryPath);
        }
    }

    std::error_code error;
    fs::remove(nativePath(m_d->temporaryPath), error);
    m_d->temporaryPath = PkString();
    m_d->isLoading = false;

    if (!successfullyLoaded) {
        ++m_d->failureCount;
        if (m_d->failureCount >= 3) {
            loadingFailed();
        } else {
            m_d->fileChangedTimer.start(500ms, [this] { fileChangedCompressed(); }, true);
        }
    } else {
        loadingFinished(loadResult.paintDevice,
                        loadResult.xRes,
                        loadResult.yRes,
                        loadResult.size);
    }
}
