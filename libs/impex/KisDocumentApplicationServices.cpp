/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisDocumentApplicationServices.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include <PkEventLoop.h>
#include <PkMutex.h>

#include <KoColor.h>
#include <KoUpdater.h>
#include <kis_image.h>

namespace
{
KisDocumentApplicationServices *s_services = nullptr;
KisDocumentApplicationServices s_headlessServices;

// 原 Qt 的 homePath()（目录工具类）的 std::filesystem 替代：C++17 无
// home_directory_path()，用 $HOME 环境变量（照 libs/resources/KoResourcePaths.cpp
// 的 environment("HOME")）。未设置时返回空 PkString（原 Qt 在 Unix 上会退回
// passwd 条目——行为变化登记见 task-3-report）。
PkString homeDirectory()
{
    const char *home = std::getenv("HOME");
    return home ? PkString(home) : PkString();
}
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

KoUpdaterPtr KisDocumentApplicationServices::createUpdater(const PkString &, UpdaterMode)
{
    return {};
}

void KisDocumentApplicationServices::waitForMutexWithFeedback(PkMutex &mutex, const PkString &)
{
    while (!mutex.tryLock()) {
        // 原 Qt 的事件泵调用（processEvents(ExcludeUserInputEvents)）。
        // PkEventLoop::processEvents() 只处理入口时队列快照、无
        // ExcludeUserInputEvents 概念（pk/concurrent）；这是主线程自身泵上的
        // 显式驱动，非跨线程投递，无需 R-30 pump 安装。
        PkEventLoop::processEvents();
    }
    mutex.unlock();
}

KisDocumentApplicationServices::RecoveryChoice
KisDocumentApplicationServices::chooseNamedAutosave(const PkString &, const PkString &)
{
    return RecoveryChoice::Cancel;
}

void KisDocumentApplicationServices::showDocumentMessage(const DocumentMessage &)
{
}

void KisDocumentApplicationServices::addRecentFile(const PkString &)
{
}

bool KisDocumentApplicationServices::queryClose(KisDocument *)
{
    return true;
}

PkString KisDocumentApplicationServices::autoSaveLocation() const
{
#if defined(_WIN32)
    // 原 Qt 的 tempPath()（目录工具类）：std::filesystem::temp_directory_path() 在
    // Windows 上依次试 TMP/TEMP/USERPROFILE/Windows 目录，语义与 Qt 一致；失败
    // 时抛 filesystem_error（Qt 版返回回退路径——headless 不覆盖该路径，登记）。
    // 注意：返回的是 ANSI 窄编码路径，非 Qt 的 UTF-16（壳内不跑 Windows 分支）。
    return PkString(std::filesystem::temp_directory_path().string().c_str());
#elif defined(__ANDROID__)
    // 原 Qt 的 writableLocation(DocumentsLocation) 在 Android 上解析到应用可写的
    // Documents 目录；headless 实现无 Android 文件系统知识，退化为
    // $HOME/Documents/krita-backup（行为变化，登记 S9 交接——桌面壳适配器经
    // setInstance 注入的仍是权威）。
    const PkString path = homeDirectory() + PkString("/Documents/krita-backup");
    if (!std::filesystem::exists(path.PkToUtf8())) {
        std::filesystem::create_directories(path.PkToUtf8());
    }
    return path;
#else
    return homeDirectory();
#endif
}

PkImage KisDocumentApplicationServices::previewCheckerboard(int tileSize) const
{
    const int size = std::max(1, tileSize);
    PkImage image(size * 2, size * 2, PkImage::Format_RGB32);
    // 原 image.fill(白色) 的等价：Format_RGB32 像素布局 0xffRRGGBB，
    // 白 = 0xFFFFFFFF（ARGB32 打包值，fill(uint32_t) 按像素打包约定写入）。
    image.fill(0xFFFFFFFFu);
    for (int y = 0; y < image.height(); ++y) {
        // 原行指针是 32 位打包色值类型（即 uint32_t），直接换 uint32_t*。
        uint32_t *line = reinterpret_cast<uint32_t *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (((x / size) + (y / size)) % 2) {
                // 原 qRgb(192,192,192) = 0xFFC0C0C0（ARGB32 打包值）。
                line[x] = 0xFFC0C0C0u;
            }
        }
    }
    return image;
}

bool KisDocumentApplicationServices::securityBookmarksEnabled() const
{
    return false;
}

bool KisDocumentApplicationServices::parentDirectoryHasPermissions(const PkString &) const
{
    return true;
}

void KisDocumentApplicationServices::createSavedFileBookmark(const PkString &)
{
}

std::unique_ptr<KisDocumentBusyCursor> KisDocumentApplicationServices::createBusyCursor()
{
    return {};
}

std::optional<PkList<KoColor>> KisDocumentApplicationServices::activeColorHistory() const
{
    return std::nullopt;
}

PkColor KisDocumentApplicationServices::defaultAssistantsColor() const
{
    return PkColor(176, 176, 176, 255);
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

PkString KisDocumentApplicationServices::backupFileSuffix() const
{
    return PkString("~");
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

void KisDocumentApplicationServices::setDefaultGridSpacing(const PkPoint &)
{
}

void KisDocumentApplicationServices::storeNewImageDefaults(qint32,
                                                           qint32,
                                                           qreal,
                                                           const PkString &,
                                                           const PkString &,
                                                           const PkString &)
{
}
