#include "PkFileStream.h"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

PkFileStream::PkFileStream()
    : m_fd(-1)
{
}

PkFileStream::PkFileStream(const PkString &filePath)
    : m_filePath(filePath)
    , m_fd(-1)
{
}

PkFileStream::PkFileStream(const char *filePath)
    : m_filePath(filePath)
    , m_fd(-1)
{
}

// 基类析构故意不隐式 close()（见 PkStream.cpp 注释），关闭是具体子类的责任。
PkFileStream::~PkFileStream()
{
    close();
}

PkString PkFileStream::fileName() const
{
    return m_filePath;
}

bool PkFileStream::open(OpenMode mode)
{
    // 已打开则先关掉——「close() 之后重新 open() 得到干净状态」的语义由
    // setOpenMode() 一次性收住（游标清零、unget 缓冲清空、错误文案清空）。
    close();

    const bool read  = (mode & ReadOnly) != 0;
    const bool write = (mode & WriteOnly) != 0;

    int flags = 0;
    if (read && write) {
        flags |= O_RDWR;
    } else if (read) {
        flags |= O_RDONLY;
    } else if (write) {
        flags |= O_WRONLY;
    } else {
        setErrorString("PkFileStream::open: cannot open file without PkStream::ReadOnly or PkStream::WriteOnly");
        return false;
    }

    if (write) {
        flags |= O_CREAT;
        if (mode & Append) {
            flags |= O_APPEND;
        }
        if (mode & Truncate) {
            flags |= O_TRUNC;
        }
    }
    flags |= O_CLOEXEC;

    m_fd = ::open(m_filePath.PkToUtf8().c_str(), flags, 0666);
    if (m_fd < 0) {
        setErrorString(PkString("PkFileStream::open: ") + PkString(std::strerror(errno)));
        return false;
    }

    setOpenMode(mode);
    return true;
}

void PkFileStream::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    setOpenMode(NotOpen);
}

bool PkFileStream::flush()
{
    // 原始 POSIX write() 没有用户态缓冲，数据直接进内核，没有可 flush 的东西。
    // 真 Qt 的文件设备 flush() 也不做 fsync，这里恒成功即可。
    return m_fd >= 0;
}

PkStream::pk_int64 PkFileStream::size() const
{
    if (m_fd < 0) {
        return 0;
    }
    struct stat st;
    if (::fstat(m_fd, &st) != 0) {
        return 0;
    }
    return static_cast<pk_int64>(st.st_size);
}

bool PkFileStream::seek(pk_int64 pos)
{
    // 基类先动逻辑游标（校验 isOpen/isSequential/pos<0，清 unget 缓冲）；
    // 真文件还必须把 fd 游标也定位过去。
    if (!PkStream::seek(pos)) {
        return false;
    }
    if (::lseek(m_fd, static_cast<off_t>(pos), SEEK_SET) == static_cast<off_t>(-1)) {
        setErrorString(PkString("PkFileStream::seek: ") + PkString(std::strerror(errno)));
        return false;
    }
    return true;
}

bool PkFileStream::isSequential() const
{
    return false;
}

PkStream::pk_int64 PkFileStream::readData(char *data, pk_int64 maxSize)
{
    if (m_fd < 0) {
        return -1;
    }
    // 逻辑游标可能因 ungetChar()/peek() 回退而与 fd 游标脱节，每次读前都
    // 把 fd 定位到 pos()，再用 pos() 索引的真实偏移读。EOF 时 ::read 返回 0，
    // 原样透传——契约第 1 条「EOF 返回 0」。
    const pk_int64 p = pos();
    if (p < 0) {
        return -1;
    }
    if (::lseek(m_fd, static_cast<off_t>(p), SEEK_SET) == static_cast<off_t>(-1)) {
        return -1;
    }
    const ssize_t n = ::read(m_fd, data, static_cast<size_t>(maxSize));
    if (n < 0) {
        return -1;
    }
    return static_cast<pk_int64>(n);
}

PkStream::pk_int64 PkFileStream::writeData(const char *data, pk_int64 maxSize)
{
    if (m_fd < 0) {
        return -1;
    }
    // Append 模式下 O_APPEND 保证写永远到文件末尾（不需要也不应该 lseek）；
    // 非 Append 模式按逻辑游标定位后写。写短了（磁盘满等）返回实际字节数，
    // 不在这里重试——契约第 2 条「短读不补零」的写侧同一条。
    if (!(openMode() & Append)) {
        const pk_int64 p = pos();
        if (p < 0) {
            return -1;
        }
        if (::lseek(m_fd, static_cast<off_t>(p), SEEK_SET) == static_cast<off_t>(-1)) {
            return -1;
        }
    }
    const ssize_t n = ::write(m_fd, data, static_cast<size_t>(maxSize));
    if (n < 0) {
        return -1;
    }
    return static_cast<pk_int64>(n);
}
