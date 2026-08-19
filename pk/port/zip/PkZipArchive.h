#pragma once

#include <cstdint>
#include <string>
#include <vector>

// PkString 归 R-01（pk/string），已交付；PkStream 归 R-12 Task 1
// （pk/port/PkStream.h）。本头文件的方法按 const 引用传 PkString、按指针传/
// 返回 PkStream——都只需要前置声明，完整定义放在 .cpp 里（同
// PkResourceStorage.h/PkStream.h 的既有做法）。
class PkString;
class PkStream;

// PkZipArchive —— 零 Qt 依赖的 zip 归档端口，替换 QuaZip（Q-5，
// docs/Qt替代品选型.md）。底层用 minizip-ng（4.0.7，见 pk/port/zip/CMakeLists.txt
// 的 FetchContent 钉版）。
//
// 能力面 1:1 对应 libs/store/KoQuaZipStore.cpp + libs/resources/
// KisResourceStorage.cpp:140 这 2 个文件——保留范围内全部的 QuaZip 消费者
// （R-12 Task 6 brief「能力面」表），一项不多：
//
//   打开（文件名 / 任意 PkStream）、setZip64Enabled、UTF-8 文件名（恒定，不
//   开关）、setAutoClose、setDataDescriptorWritingEnabled、条目计数、定位
//   条目、目录列举、条目读写各产出一个 PkStream、错误码。**不实现加密**
//   （QuaZip::setPassword 在保留范围内实测 0 处调用）。
//
// ── 条目流的所有权 ──────────────────────────────────────────────
// openEntryForRead()/openEntryForWrite() 返回的 PkStream* 是 new 出来的，
// 归调用方所有，用完必须 delete——语义对齐 KoQuaZipStore::Private::currentFile
// 的所有权归属（KoQuaZipStore.cpp 88-92：析构里显式 delete）。
//
// ── 同一时刻只能有一个条目开着 ──────────────────────────────────
// 这是从真 QuaZip 继承来的真实约束，不是本类新引入的限制——
// KoQuaZipStore.cpp 76-86 的析构注释原文引用了 QuaZipFile 的文档：
// "do not close \c zip object or change its current file as long as
// QuaZipFile is open"。openEntryForRead()/openEntryForWrite() 在已有条目
// 开着时直接返回 nullptr；close() 在有条目开着时直接返回 false。
class PkZipArchive
{
public:
    // KoStore::Mode（Read/Write）→ KoQuaZipStore::init() 行156：
    // `d->good = dd->archive->open(d->mode == Write ? QuaZip::mdCreate : QuaZip::mdUnzip)`。
    enum Mode { Read, Write };

    explicit PkZipArchive(Mode mode);
    ~PkZipArchive();

    // 内部持有 minizip-ng 裸句柄与（PkStream 打开路径下）不可平凡拷贝的流
    // 包装体——禁止拷贝，语义上也没有"拷贝一个正在写的归档"这种用法。
    PkZipArchive(const PkZipArchive &) = delete;
    PkZipArchive &operator=(const PkZipArchive &) = delete;

    // KoQuaZipStore::KoQuaZipStore(const QString &_filename, ...) 行43-52：
    // dd->archive = new QuaZip(_filename)。Mode 决定用 MZ_OPEN_MODE_READ 还是
    // MZ_OPEN_MODE_WRITE|MZ_OPEN_MODE_CREATE 打开底层文件流。
    bool openFile(const PkString &path);

    // KoQuaZipStore::KoQuaZipStore(QIODevice *dev, ...) 行54-60：
    // dd->archive = new QuaZip(dev)——这正是"zip 归档本身建在 PkStream 之上"
    // 这条能力的落点。**不接管 stream 的生命周期**（是否在 close() 时关闭它
    // 由 setAutoClose() 决定），调用方必须保证 stream 在归档使用期间保持存活。
    // **调用前 stream 必须已经处于 open 状态**（本类不会替它调 open()）——
    // 实测过 minizip-ng 4.0.7 的 mz_zip_open()（mz_zip.c）从不对传入的
    // stream 调 ->vtbl->open，只会直接 read/write/seek/tell 它。
    bool openStream(PkStream *stream);

    // KoQuaZipStore::doFinalize() 行181-191（好路径）+ 析构行62-93（兜底）。
    // 有条目还开着时返回 false、不做任何事——见类头注释「同一时刻只能有一个
    // 条目开着」。
    bool close();

    bool isOpen() const;

    // KoQuaZipStore::init() 行150：dd->archive->setDataDescriptorWritingEnabled(false)。
    // 真实调用点只传 false，这里仍然把开关暴露出来（QuaZip 本身是可读写的
    // setter），语义对齐、不额外收窄。必须在 openFile()/openStream() 之前调用
    // 才生效（对应真调用点 init() 在 archive->open() 之前调这个 setter）。
    void setDataDescriptorWritingEnabled(bool enabled);

    // KoQuaZipStore::init() 行151：dd->archive->setZip64Enabled(enableZip64)。
    // minizip-ng 没有归档级别的 zip64 开关（archive 级只有
    // mz_zip_set_data_descriptor 这一个 setter，见 mz_zip.h）——zip64 是
    // per-entry 字段（mz_zip_file::zip64，MZ_ZIP64_AUTO/FORCE/DISABLE，见
    // mz.h）。这里缓存的布尔值在 openEntryForWrite() 时应用到每个条目：
    // true → MZ_ZIP64_FORCE，false（默认）→ MZ_ZIP64_AUTO——AUTO 是
    // minizip-ng 自己的默认值，不是本类发明的兜底。必须在 openFile()/
    // openStream() 之前调用才生效。
    void setZip64Enabled(bool enabled);

    // KoQuaZipStore::init() 行154：
    // dd->usingSaveFile = ...; dd->archive->setAutoClose(!dd->usingSaveFile)。
    // 只影响 openStream() 打开的归档：close() 时是否连带调用
    // stream->close()。openFile() 打开的归档不受这个开关影响——文件流本来
    // 就是本类自己创建的，close() 总是会关（对应 QuaZip(_filename) 那条
    // 构造路径下设备生命周期完全归 QuaZip 自己管）。默认 true。
    void setAutoClose(bool autoClose);

    // KoQuaZipStore::init() 行176-177：
    // debugStore << dd->archive->getEntriesCount() << directoryList();
    // d->good = dd->archive->getEntriesCount()。归档未打开时返回 -1。
    int64_t entryCount() const;

    // KoQuaZipStore::openRead() 行238：
    // dd->archive->setCurrentFile(fixedPath)。定位失败（条目不存在）返回
    // false；调用 openEntryForRead() 前必须先成功调用这个方法。
    bool locateEntry(const PkString &name);

    // KoQuaZipStore::directoryList() 行126-138：Read 模式下
    // dd->archive->getFileNameList()；QuaZipDir（KoQuaZipStore.cpp:291 的
    // enterAbsoluteDirectory()，能力面表里唯一的"目录列举"调用点）本身也是
    // 基于这份平铺的条目名单做前缀存在性判断，不是一个独立的能力。每次调用
    // 都重新扫一遍中央目录（真 QuaZip 只在 Read 模式缓存，Write 模式同样是
    // "每次重新取"——见行137 else 分支），本类不做缓存优化，与 Write 模式行为
    // 一致、Read 模式功能等价（不缓存不影响正确性，只影响调用开销）。
    std::vector<PkString> entryNames() const;

    // KoQuaZipStore::openRead() 行243-248：
    // dd->currentFile = new QuaZipFile(dd->archive);
    // dd->currentFile->open(QIODevice::ReadOnly);
    // d->stream = dd->currentFile —— 条目本身就是一个 PkStream。
    // 必须先 locateEntry() 定位成功；失败（未定位 / 已有条目开着 / 底层
    // entry_read_open 失败）返回 nullptr。
    // 返回流的 size()：Read 模式 = 条目解压后大小；Write 模式恒 0（见
    // PkZipEntryStream 类头注释）。
    PkStream *openEntryForRead();

    // KoQuaZipStore::openWrite() 行193-216：
    // QuaZipNewInfo newInfo(fixedPath); newInfo.setPermissions(...);
    // currentFile->open(WriteOnly, newInfo, 0, 0, Z_DEFLATED, dd->compressionLevel)。
    // compressionEnabled 对应 setCompressionEnabled() 行95-104 的两档：
    // true→Z_DEFAULT_COMPRESSION、false→Z_NO_COMPRESSION（真实调用点里压缩
    // 方法恒为 Z_DEFLATED，只有 level 在这两档间切）。unixPermissions 是原始
    // POSIX 权限位（真实调用点固定传
    // QFileDevice::ReadOwner|ReadGroup|ReadOther，即 0444；这里放开成参数，
    // 因为 QuaZipNewInfo::setPermissions 本身就是通用 setter）。失败（已有
    // 条目开着 / 底层 entry_write_open 失败）返回 nullptr。
    PkStream *openEntryForWrite(const PkString &name, uint32_t unixPermissions, bool compressionEnabled);

    // KoQuaZipStore::doFinalize() 行189：
    // dd->archive->getZipError() == ZIP_OK。返回最近一次归档级操作
    // （openFile/openStream/close/locateEntry/openEntryFor*，以及条目流
    // 读写/关闭时探测到的失败）的 minizip-ng 错误码，MZ_OK(0) 表示成功。
    int lastError() const;
    bool isOk() const { return lastError() == 0; /* MZ_OK */ }

private:
    struct Impl;
    Impl *m_impl;
};
