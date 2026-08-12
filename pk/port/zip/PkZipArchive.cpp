#include "PkZipArchive.h"

#include "../PkStream.h"
#include "PkString.h"

#include <mz.h>
#include <mz_os.h>
#include <mz_strm.h>
#include <mz_strm_os.h>
#include <mz_zip.h>

#include <cstring>
#include <ctime>

namespace {

// ── 自定义 mz_stream：把一个 PkStream* 包成 minizip-ng 认得的流 ──────────
//
// 布局照抄 minizip-ng 自己的约定（mz_strm_mem.c 的 mz_stream_mem_s）：第一个
// 成员必须是 `mz_stream`（vtbl 指针打头），因为 mz_stream_read()/write() 等
// 全部 dispatcher（mz_strm.c）都是把调用方传入的 void* 直接
// reinterpret_cast<mz_stream*> 去读 ->vtbl->read 等函数指针。
//
// 实测过 minizip-ng 4.0.7 的 mz_zip_open()（mz_zip.c）：它只用
// mz_stream_read/write/tell/seek 操作传入的 stream，从不对它调 ->vtbl->open
// 或 ->vtbl->close——那两个回调只在"用一个路径字符串自己开流"的场景
// （mz_stream_os_open 那条路径）才会被间接触发，我们直接把已经 open()
// 过的 PkStream* 塞进来，所以这两个回调实际上永远不会被 minizip-ng 调用；
// 仍然给出安全实现只是为了让 vtbl 签名完整、不留没定义的函数指针。
struct PkMzStreamWrapper {
    mz_stream stream;
    PkStream *pkStream; // 不持有所有权，见 PkZipArchive::openStream() 的注释。
};

int32_t PkMz_open(void *strm, const char * /*path*/, int32_t /*mode*/)
{
    PkMzStreamWrapper *s = static_cast<PkMzStreamWrapper *>(strm);
    return (s->pkStream && s->pkStream->isOpen()) ? MZ_OK : MZ_OPEN_ERROR;
}

int32_t PkMz_is_open(void *strm)
{
    PkMzStreamWrapper *s = static_cast<PkMzStreamWrapper *>(strm);
    return (s->pkStream && s->pkStream->isOpen()) ? MZ_OK : MZ_STREAM_ERROR;
}

int32_t PkMz_read(void *strm, void *buf, int32_t size)
{
    PkMzStreamWrapper *s = static_cast<PkMzStreamWrapper *>(strm);
    const PkStream::pk_int64 n = s->pkStream->read(static_cast<char *>(buf), size);
    return n < 0 ? MZ_STREAM_ERROR : static_cast<int32_t>(n);
}

int32_t PkMz_write(void *strm, const void *buf, int32_t size)
{
    PkMzStreamWrapper *s = static_cast<PkMzStreamWrapper *>(strm);
    const PkStream::pk_int64 n = s->pkStream->write(static_cast<const char *>(buf), size);
    return n < 0 ? MZ_WRITE_ERROR : static_cast<int32_t>(n);
}

int64_t PkMz_tell(void *strm)
{
    PkMzStreamWrapper *s = static_cast<PkMzStreamWrapper *>(strm);
    return s->pkStream->pos();
}

int32_t PkMz_seek(void *strm, int64_t offset, int32_t origin)
{
    PkMzStreamWrapper *s = static_cast<PkMzStreamWrapper *>(strm);
    PkStream::pk_int64 target = 0;
    switch (origin) {
    case MZ_SEEK_SET:
        target = offset;
        break;
    case MZ_SEEK_CUR:
        target = s->pkStream->pos() + offset;
        break;
    case MZ_SEEK_END:
        target = s->pkStream->size() + offset;
        break;
    default:
        return MZ_SEEK_ERROR;
    }
    return s->pkStream->seek(target) ? MZ_OK : MZ_SEEK_ERROR;
}

int32_t PkMz_close(void * /*strm*/)
{
    // 见结构体注释：minizip-ng 从不主动调这个回调去关闭调用方传入的
    // stream。真正的 autoClose 逻辑在 PkZipArchive::close() 里。
    return MZ_OK;
}

int32_t PkMz_error(void * /*strm*/)
{
    return MZ_OK;
}

mz_stream_vtbl g_pkMzVtbl = {
    PkMz_open, PkMz_is_open, PkMz_read, PkMz_write, PkMz_tell, PkMz_seek,
    PkMz_close, PkMz_error,
    nullptr, nullptr, // create/destroy：本类自己管理生命周期，不用这两个通用工厂。
    nullptr, nullptr  // get/set_prop_int64：append 模式用得到，本任务范围不覆盖 append。
};

inline int32_t clampToInt32(PkStream::pk_int64 n)
{
    return n > INT32_MAX ? INT32_MAX : static_cast<int32_t>(n);
}

// ── 条目流：读/写各一个条目的数据，包成一个 PkStream ──────────────────
//
// KoQuaZipStore.cpp 里 QuaZipFile 本身就是一个 QIODevice（openRead() 行248：
// d->stream = dd->currentFile）——这个类是它的对应物。zip 条目的解压/压缩
// 流本质上是顺序的（minizip-ng 的 entry_read/entry_write 不支持中途任意
// seek），isSequential()==true，对齐真 QuaZipFile 的语义。
//
// 析构里补一次 close()（不依赖调用方记得调用）：PkStream 基类故意不这么做
// （见 PkStream.cpp 头注释），但这里必须做——不调用 entry_write_close()，
// 写入的数据根本不会被写进中央目录，zip 文件是坏的。
class PkZipEntryStream : public PkStream {
public:
    // zipHandle：mz_zip_create() 出来的裸句柄（PkZipArchive::Impl::zip）。
    // entryOpenFlag：指向 PkZipArchive::Impl::entryOpen，条目关闭时清掉，
    //   实现"同一时刻只能有一个条目开着"这条从真 QuaZip 继承来的约束
    //   （类头注释）。lastErrorOut：指向 Impl::lastError，条目读写/关闭的
    //   失败一并反映到 PkZipArchive::lastError()。三者都不持有所有权，
    //   生命周期由 PkZipArchive 保证（entry 必须在 archive 之前销毁——同
    //   "同一时刻只能有一个条目开着"的约束）。
    PkZipEntryStream(void *zipHandle, bool forWrite, bool *entryOpenFlag, int32_t *lastErrorOut)
        : m_zip(zipHandle)
        , m_forWrite(forWrite)
        , m_entryOpenFlag(entryOpenFlag)
        , m_lastErrorOut(lastErrorOut)
        , m_lowLevelClosed(false)
    {
        open(forWrite ? WriteOnly : ReadOnly);
    }

    ~PkZipEntryStream() override { close(); }

    bool isSequential() const override { return true; }

    void close() override
    {
        if (!m_lowLevelClosed) {
            int32_t rc;
            if (m_forWrite) {
                // crc32/compressed_size/uncompressed_size 传 -1（不是 0！）
                // 才会触发 mz_zip_entry_write_close() 内部"从 compress_stream
                // 的 TOTAL_IN/TOTAL_OUT 读真实值"这条路径（mz_zip.c 源码：
                // `if (compressed_size < 0) mz_stream_get_prop_int64(...)`）；
                // 传 0 会被当成"确实是 0 字节"的字面值直接写进本地头/中央
                // 目录，产出一个内容对不上、看似合法实际损坏的 zip。crc32
                // 参数本身在非 raw 模式下会被内部忽略、用
                // zip->entry_crc32 覆盖（同源码：
                // `if (!zip->entry_raw) crc32 = zip->entry_crc32;`），传什么
                // 都无所谓，这里传 0 只是占位。
                rc = mz_zip_entry_write_close(m_zip, 0, -1, -1);
            } else {
                rc = mz_zip_entry_read_close(m_zip, nullptr, nullptr, nullptr);
            }
            if (m_lastErrorOut) {
                *m_lastErrorOut = rc;
            }
            m_lowLevelClosed = true;
            if (m_entryOpenFlag) {
                *m_entryOpenFlag = false;
            }
        }
        PkStream::close();
    }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        if (m_forWrite) {
            return -1;
        }
        const int32_t n = mz_zip_entry_read(m_zip, data, clampToInt32(maxSize));
        if (n < 0 && m_lastErrorOut) {
            *m_lastErrorOut = n;
        }
        return n < 0 ? -1 : n;
    }

    pk_int64 writeData(const char *data, pk_int64 maxSize) override
    {
        if (!m_forWrite) {
            return -1;
        }
        const int32_t n = mz_zip_entry_write(m_zip, data, clampToInt32(maxSize));
        if (n < 0 && m_lastErrorOut) {
            *m_lastErrorOut = n;
        }
        return n < 0 ? -1 : n;
    }

private:
    void *m_zip;
    bool m_forWrite;
    bool *m_entryOpenFlag;
    int32_t *m_lastErrorOut;
    bool m_lowLevelClosed;
};

} // namespace

struct PkZipArchive::Impl {
    Mode mode;
    void *zip = nullptr;           // mz_zip_create()
    void *ownedOsStream = nullptr; // openFile() 路径：mz_stream_os_create()，本类持有并负责销毁
    PkMzStreamWrapper customStream{};
    bool usingCustomStream = false;
    bool open = false;
    bool entryOpen = false;
    bool zip64Enabled = false;
    bool dataDescriptorEnabled = false; // 对应真调用点固定传 false（KoQuaZipStore.cpp:150）
    bool autoCloseStream = true;
    int32_t lastError = MZ_OK;

    explicit Impl(Mode m) : mode(m) {}

    bool openOnStream(void *mzStream)
    {
        zip = mz_zip_create();
        if (!zip) {
            lastError = MZ_MEM_ERROR;
            return false;
        }
        // 必须在 mz_zip_open() 之前调，对齐真调用点 init() 里
        // setDataDescriptorWritingEnabled() 排在 archive->open() 之前
        // （KoQuaZipStore.cpp:150 vs :156）。
        mz_zip_set_data_descriptor(zip, dataDescriptorEnabled ? 1 : 0);

        const int32_t omode = (mode == Write) ? (MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_CREATE)
                                               : MZ_OPEN_MODE_READ;
        const int32_t rc = mz_zip_open(zip, mzStream, omode);
        lastError = rc;
        if (rc != MZ_OK) {
            mz_zip_delete(&zip);
            return false;
        }
        open = true;
        return true;
    }
};

PkZipArchive::PkZipArchive(Mode mode) : m_impl(new Impl(mode)) {}

PkZipArchive::~PkZipArchive()
{
    // 兜底同 KoQuaZipStore 析构（KoQuaZipStore.cpp:70-72）：忘了显式 close()
    // 时补一次；有条目还开着这里没法安全处理（同真 QuaZip 的固有限制，见
    // 类头注释），close() 会直接返回 false、什么也不做，剩下的裸句柄泄漏
    // 由调用方没遵守"先关条目再关归档"这条约束负责，不是本类能兜的底。
    close();
    delete m_impl;
}

bool PkZipArchive::openFile(const PkString &path)
{
    if (m_impl->open) {
        return false;
    }
    void *osStream = mz_stream_os_create();
    if (!osStream) {
        m_impl->lastError = MZ_MEM_ERROR;
        return false;
    }
    const std::string utf8Path = path.PkToUtf8();
    const int32_t omode = (m_impl->mode == Write) ? (MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_CREATE)
                                                   : MZ_OPEN_MODE_READ;
    const int32_t rc = mz_stream_os_open(osStream, utf8Path.c_str(), omode);
    if (rc != MZ_OK) {
        m_impl->lastError = rc;
        mz_stream_os_delete(&osStream);
        return false;
    }
    m_impl->ownedOsStream = osStream;
    if (!m_impl->openOnStream(osStream)) {
        mz_stream_os_close(osStream);
        mz_stream_os_delete(&osStream);
        m_impl->ownedOsStream = nullptr;
        return false;
    }
    return true;
}

bool PkZipArchive::openStream(PkStream *stream)
{
    if (m_impl->open || !stream) {
        return false;
    }
    // 评审 I-1：头注释写了「调用前 stream 必须已经处于 open 状态」，但 minizip-ng
    // 从不对传入的 stream 调 ->vtbl->open——契约没有任何东西真的拦。未 open 的
    // stream 会在 openEntryForWrite() 才失败、产出 0 字节归档，错误现在拦在门口。
    if (!stream->isOpen()) {
        return false;
    }
    m_impl->customStream.stream.vtbl = &g_pkMzVtbl;
    m_impl->customStream.stream.base = nullptr;
    m_impl->customStream.pkStream = stream;
    m_impl->usingCustomStream = true;
    if (!m_impl->openOnStream(&m_impl->customStream)) {
        m_impl->usingCustomStream = false;
        return false;
    }
    return true;
}

bool PkZipArchive::close()
{
    if (!m_impl->open) {
        return false;
    }
    if (m_impl->entryOpen) {
        // 同一时刻只能有一个条目开着——见类头注释。调用方违反了这条约束，
        // 拒绝执行，不去冒险动一个还在读/写的条目底下的归档。
        return false;
    }

    const int32_t rc = mz_zip_close(m_impl->zip);
    mz_zip_delete(&m_impl->zip);
    m_impl->lastError = rc;

    if (m_impl->ownedOsStream) {
        mz_stream_os_close(m_impl->ownedOsStream);
        mz_stream_os_delete(&m_impl->ownedOsStream);
    } else if (m_impl->usingCustomStream && m_impl->autoCloseStream) {
        m_impl->customStream.pkStream->close();
    }
    m_impl->usingCustomStream = false;
    m_impl->open = false;
    return rc == MZ_OK;
}

bool PkZipArchive::isOpen() const
{
    return m_impl->open;
}

void PkZipArchive::setDataDescriptorWritingEnabled(bool enabled)
{
    m_impl->dataDescriptorEnabled = enabled;
}

void PkZipArchive::setZip64Enabled(bool enabled)
{
    m_impl->zip64Enabled = enabled;
}

void PkZipArchive::setAutoClose(bool autoClose)
{
    m_impl->autoCloseStream = autoClose;
}

int64_t PkZipArchive::entryCount() const
{
    if (!m_impl->open) {
        return -1;
    }
    uint64_t n = 0;
    if (mz_zip_get_number_entry(m_impl->zip, &n) != MZ_OK) {
        return -1;
    }
    return static_cast<int64_t>(n);
}

bool PkZipArchive::locateEntry(const PkString &name)
{
    if (!m_impl->open) {
        return false;
    }
    const std::string utf8Name = name.PkToUtf8();
    const int32_t rc = mz_zip_locate_entry(m_impl->zip, utf8Name.c_str(), /*ignore_case=*/0);
    m_impl->lastError = rc;
    return rc == MZ_OK;
}

std::vector<PkString> PkZipArchive::entryNames() const
{
    std::vector<PkString> names;
    if (!m_impl->open) {
        return names;
    }
    // 评审 I-2：这是个 const 方法，但底下 mz_zip_goto_first/next_entry() 会
    // 把 minizip-ng 的「当前条目」游标推到列表尾——不还原的话，调用方之前
    // locateEntry() 定位好的条目就被悄悄改掉了（m_entryOpen 那道防护管不到
    // 这个游标）。先存游标、遍历完再还原，让 entryNames() 真的表现成只读。
    const int64_t savedEntry = mz_zip_get_entry(m_impl->zip);
    int32_t rc = mz_zip_goto_first_entry(m_impl->zip);
    while (rc == MZ_OK) {
        mz_zip_file *info = nullptr;
        if (mz_zip_entry_get_info(m_impl->zip, &info) == MZ_OK && info && info->filename) {
            names.push_back(PkString::PkFromUtf8(info->filename, static_cast<int>(std::strlen(info->filename))));
        }
        rc = mz_zip_goto_next_entry(m_impl->zip);
    }
    mz_zip_goto_entry(m_impl->zip, savedEntry);
    return names;
}

PkStream *PkZipArchive::openEntryForRead()
{
    if (!m_impl->open || m_impl->mode != Read || m_impl->entryOpen) {
        return nullptr;
    }
    const int32_t rc = mz_zip_entry_read_open(m_impl->zip, /*raw=*/0, /*password=*/nullptr);
    m_impl->lastError = rc;
    if (rc != MZ_OK) {
        return nullptr;
    }
    m_impl->entryOpen = true;
    return new PkZipEntryStream(m_impl->zip, /*forWrite=*/false, &m_impl->entryOpen, &m_impl->lastError);
}

PkStream *PkZipArchive::openEntryForWrite(const PkString &name, uint32_t unixPermissions, bool compressionEnabled)
{
    if (!m_impl->open || m_impl->mode != Write || m_impl->entryOpen) {
        return nullptr;
    }

    const std::string utf8Name = name.PkToUtf8();

    mz_zip_file info;
    std::memset(&info, 0, sizeof(info));
    info.filename = utf8Name.c_str();
    info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
    info.flag = MZ_ZIP_FLAG_UTF8; // KoQuaZipStore.cpp:152 setFileNameCodec("UTF-8") 的对应位。
    info.version_madeby = MZ_VERSION_MADEBY;
    info.external_fa = (unixPermissions << 16); // unix 权限的标准编码位置，见 mz_os.h MZ_VERSION_MADEBY_HOST_SYSTEM。
    info.zip64 = m_impl->zip64Enabled ? MZ_ZIP64_FORCE : MZ_ZIP64_AUTO;
    info.modified_date = std::time(nullptr);

    const int16_t level = compressionEnabled ? MZ_COMPRESS_LEVEL_DEFAULT : 0; // 0 == Z_NO_COMPRESSION。

    const int32_t rc = mz_zip_entry_write_open(m_impl->zip, &info, level, /*raw=*/0, /*password=*/nullptr);
    m_impl->lastError = rc;
    if (rc != MZ_OK) {
        return nullptr;
    }
    m_impl->entryOpen = true;
    return new PkZipEntryStream(m_impl->zip, /*forWrite=*/true, &m_impl->entryOpen, &m_impl->lastError);
}

int PkZipArchive::lastError() const
{
    return m_impl->lastError;
}
