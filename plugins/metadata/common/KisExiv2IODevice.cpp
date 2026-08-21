/*
 * SPDX-FileCopyrightText: 2022 Sharaf Zaman <shzam@sdf.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisExiv2IODevice.h"

#include "kis_debug.h"

#include <fcntl.h>
#include <filesystem>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

KisExiv2IODevice::KisExiv2IODevice(PkString path)
    : m_file(path)
    , m_mappedArea(nullptr)
{
}

KisExiv2IODevice::~KisExiv2IODevice()
{
    m_file.close();
}

int KisExiv2IODevice::open()
{
    if (m_file.isOpen()) {
        m_file.close();
    }

    // return zero if successful
    const bool ok = m_file.open(PkStream::ReadWrite);
    m_error = ok ? 0 : 1;
    return ok ? 0 : 1;
}

int KisExiv2IODevice::close()
{
    if (munmap() != 0) {
        return 1;
    }
    m_file.close();
    return 0;
}

#if EXIV2_TEST_VERSION(0,28,0)
size_t KisExiv2IODevice::write(const Exiv2::byte *data, size_t wcount)
#else
long KisExiv2IODevice::write(const Exiv2::byte *data, long wcount)
#endif
{
    if (!m_file.isWritable()) {
        qWarning() << "KisExiv2IODevice: File not open for writing.";
        return 0;
    }
    const std::int64_t writeCount = m_file.write(reinterpret_cast<const char *>(data), wcount);
    if (writeCount > 0) {
        return writeCount;
    }

    return 0;
}

#if EXIV2_TEST_VERSION(0,28,0)
size_t KisExiv2IODevice::write(Exiv2::BasicIo &src)
#else
long KisExiv2IODevice::write(Exiv2::BasicIo &src)
#endif
{
    if (static_cast<BasicIo *>(this) == &src) {
        return 0;
    }
    if (!src.isopen()) {
        return 0;
    }
    if (!m_file.isWritable()) {
        qWarning() << "KisExiv2IODevice: File not open for writing.";
        return 0;
    }
    Exiv2::byte buffer[4096];
    long readCount = 0;
    long totalWriteCount = 0;
    while ((readCount = src.read(buffer, sizeof(buffer))) != 0) {
        totalWriteCount += write(buffer, readCount);
    }

    return totalWriteCount;
}

int KisExiv2IODevice::putb(Exiv2::byte data)
{
    if (!m_file.isWritable()) {
        qWarning() << "KisExiv2IODevice: File not open for writing.";
        return 0;
    }
    if (m_file.putChar(data)) {
        return data;
    } else {
        return EOF;
    }
}

#if EXIV2_TEST_VERSION(0,28,0)
Exiv2::DataBuf KisExiv2IODevice::read(size_t rcount)
#else
Exiv2::DataBuf KisExiv2IODevice::read(long rcount)
#endif
{
    Exiv2::DataBuf buf(rcount);
#if EXIV2_TEST_VERSION(0,28,0)
    const size_t readCount = read(buf.data(), buf.size());
    buf.resize(readCount);
#else
    const long readCount = read(buf.pData_, buf.size_);
    buf.size_ = readCount;
#endif
    return buf;
}

#if EXIV2_TEST_VERSION(0,28,0)
size_t KisExiv2IODevice::read(Exiv2::byte *buf, size_t rcount)
#else
long KisExiv2IODevice::read(Exiv2::byte *buf, long rcount)
#endif
{
    const std::int64_t bytesRead = m_file.read(reinterpret_cast<char *>(buf), rcount);
    if (bytesRead > 0) {
        return bytesRead;
    } else {
        qWarning() << "KisExiv2IODevice: Couldn't read file:" << m_file.errorString();
        // some error or EOF
        return 0;
    }
}

int KisExiv2IODevice::getb()
{
    char c;
    if (m_file.getChar(&c)) {
        return c;
    } else {
        return EOF;
    }
}

void KisExiv2IODevice::transfer(Exiv2::BasicIo &src)
{
    bool isFileBased = (dynamic_cast<Exiv2::FileIo *>(&src) || dynamic_cast<KisExiv2IODevice *>(&src));
    bool useFallback = false;

    if (isFileBased) {
        const PkString srcPath = PkString(src.path().c_str());
        // use fallback if copying failed (e.g on Android :( )
        useFallback = !renameToCurrent(srcPath);
    }

    if (!isFileBased || useFallback) {
        const bool wasOpen = isopen();
        const PkStream::OpenMode oldMode = m_file.openMode();

        // this sets file positioner to the beginning.
        if (src.open() != 0) {
            qWarning() << "KisExiv2IODevice::transfer: Couldn't open src file" << src.path().c_str();
            return;
        }

        if (!open(PkStream::ReadWrite | PkStream::Truncate)) {
            qWarning() << "KisExiv2IODevice::transfer: Couldn't open dest file" << filePathQString();
            return;
        }
        write(src);
        src.close();

        if (wasOpen) {
            open(oldMode);
        } else {
            close();
        }
    }
}

#if defined(_MSC_VER) || EXIV2_TEST_VERSION(0,28,0)
int KisExiv2IODevice::seek(int64_t offset, Exiv2::BasicIo::Position position)
#else
int KisExiv2IODevice::seek(long offset, Exiv2::BasicIo::Position position)
#endif
{
    std::int64_t pos = 0;
    switch (position) {
    case Exiv2::BasicIo::beg:
        pos = offset;
        break;
    case Exiv2::BasicIo::cur:
        pos = tell() + offset;
        break;
    case Exiv2::BasicIo::end:
        pos = size() + offset;
        break;
    }
    return m_file.seek(pos);
}

Exiv2::byte *KisExiv2IODevice::mmap(bool isWriteable)
{
    (void)isWriteable;

    if (munmap() != 0) {
        qWarning() << "KisExiv2IODevice::mmap: Couldn't unmap the mapped file";
        return nullptr;
    }

    const std::string path = filePathQString().PkToUtf8();
    const int fd = ::open(path.c_str(), O_RDWR);
    if (fd < 0) {
        qWarning() << "KisExiv2IODevice::mmap: Couldn't open file for mapping" << path.c_str();
        return nullptr;
    }
    const size_t sz = static_cast<size_t>(size());
    void *addr = ::mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (addr == MAP_FAILED) {
        qWarning() << "KisExiv2IODevice::mmap: Couldn't map the file" << path.c_str();
        return nullptr;
    }
    m_mappedArea = static_cast<Exiv2::byte *>(addr);
    return m_mappedArea;
}

int KisExiv2IODevice::munmap()
{
    if (m_mappedArea) {
        const bool ok = (::munmap(m_mappedArea, static_cast<size_t>(size())) == 0);
        m_mappedArea = nullptr;
        return ok ? 0 : 1;
    }
    return 0;
}

#if EXIV2_TEST_VERSION(0,28,0)
void KisExiv2IODevice::populateFakeData()
{
    return;
}
#endif

#if EXIV2_TEST_VERSION(0,28,0)
size_t KisExiv2IODevice::tell() const
#else
long KisExiv2IODevice::tell() const
#endif
{
    return m_file.pos();
}

size_t KisExiv2IODevice::size() const
{
    if (m_file.isWritable()) {
        m_file.flush();
    }
    return static_cast<size_t>(m_file.size());
}

bool KisExiv2IODevice::isopen() const
{
    return m_file.isOpen();
}

int KisExiv2IODevice::error() const
{
    // zero if no error
    return m_error;
}

bool KisExiv2IODevice::eof() const
{
    return m_file.atEnd();
}

#if EXIV2_TEST_VERSION(0,28,0)
const std::string& KisExiv2IODevice::path() const noexcept
#else
std::string KisExiv2IODevice::path() const
#endif
{
    return filePathQString().PkToUtf8();
}

bool KisExiv2IODevice::open(PkStream::OpenMode mode)
{
    if (m_file.isOpen()) {
        m_file.close();
    }
    return m_file.open(mode);
}

bool KisExiv2IODevice::renameToCurrent(const PkString srcPath)
{
    namespace fs = std::filesystem;
    const std::string dst = filePathQString().PkToUtf8();
    const std::string src = srcPath.PkToUtf8();
    std::error_code ec;
    const auto perms = fs::status(dst, ec).permissions();
    ec.clear();
    if (fs::exists(dst, ec)) {
        fs::remove(dst, ec);
        ec.clear();
    }
    fs::rename(src, dst, ec);
    if (ec) {
        qWarning() << "KisExiv2IODevice:renameToCurrent Couldn't copy file from"
                   << src.c_str() << "to" << dst.c_str();
        return false;
    }
    fs::permissions(dst, perms, ec);
    return !ec;
}

PkString KisExiv2IODevice::filePathQString() const
{
    return m_file.fileName();
}
