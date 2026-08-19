/* This file is part of the KDE project
   SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
   SPDX-FileCopyrightText: 2000-2002 David Faure <faure@kde.org>, Werner Trobin <trobin@kde.org>
   SPDX-FileCopyrightText: 2010 C. Boemann <cbo@boemann.dk>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoStore.h"
#include "KoStore_p.h"

// 尖括号 include KoQuaZipStore.h：薄壳靠 include 路径排序用影子头（Task 6 锁内
// 的真头仍带 Qt），真实构建里 libs/store 在全局 -I 上，两种写法都解析到同一文件。
#include <KoQuaZipStore.h>
#include "KoDirectoryStore.h"

#include "PkMemoryStream.h"
#include "PkFileStream.h"

#include <StoreDebug.h>

#include <cassert>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

// When the backend is not specified and cannot be autodetected,
// default to the zip format.
#define DefaultFormat KoStore::Zip

static KoStore::Backend determineBackend(PkStream *dev)
{
    unsigned char buf[5];
    if (dev->read((char *)buf, 4) < 4)
        return DefaultFormat; // will create a "bad" store (bad()==true)
    if (buf[0] == 'P' && buf[1] == 'K' && buf[2] == 3 && buf[3] == 4)
        return KoStore::Zip;
    return DefaultFormat; // fallback
}

KoStore *KoStore::createStore(const PkString &fileName, Mode mode,
                              const PkByteArray &appIdentification,
                              Backend backend,
                              bool writeMimetype)
{
    if (backend == Auto) {
        if (mode == Write) {
            backend = DefaultFormat;
        } else {
            std::error_code ec;
            if (std::filesystem::is_directory(fileName.PkToUtf8(), ec)) {
                backend = Directory;
            } else {
                PkFileStream file(fileName);
                if (file.open(PkStream::ReadOnly)) {
                    backend = determineBackend(&file);
                } else {
                    backend = DefaultFormat; // will create a "bad" store (bad()==true)
                }
            }
        }
    }
    switch (backend) {
    case Zip:
        return new KoQuaZipStore(fileName, mode, appIdentification, writeMimetype);
    case Directory:
        return new KoDirectoryStore(fileName /* should be a dir name.... */, mode, writeMimetype);
    default:
        warnStore << "Unsupported backend requested for KoStore : " << backend;
        return 0;
    }
}

KoStore *KoStore::createStore(PkStream *device, Mode mode,
                              const PkByteArray &appIdentification,
                              Backend backend,
                              bool writeMimetype)
{
    if (backend == Auto) {
        if (mode == Write) {
            backend = DefaultFormat;
        } else {
            if (device->open(PkStream::ReadOnly)) {
                backend = determineBackend(device);
                device->close();
            }
        }
    }
    switch (backend) {
    case Directory:
        errorStore << "Can't create a Directory store for a memory buffer!";
        return 0;
    case Zip:
        return new KoQuaZipStore(device, mode, appIdentification, writeMimetype);
    default:
        warnStore << "Unsupported backend requested for KoStore : " << backend;
        return 0;
    }
}

namespace
{
const char ROOTPART[] = "root";
const char MAINNAME[] = "maindoc.xml";
}

KoStore::KoStore(Mode mode, bool writeMimetype)
    : d_ptr(new KoStorePrivate(this, mode, writeMimetype))
{
}

KoStore::~KoStore()
{
    KoStorePrivate *d = d_func();
    delete d->stream;
    delete d_ptr;
}

bool KoStore::open(const PkString &_name)
{
    KoStorePrivate *d = d_func();
    // This also converts from relative to absolute, i.e. merges the currentPath()
    d->fileName = d->toExternalNaming(_name);

    debugStore << "KOStore" << _name << d->fileName;

    if (d->isOpen) {
        warnStore << "Store is already opened, missing close";
        return false;
    }

    if (d->fileName.size() > 512) {
        errorStore << "KoStore: Filename " << d->fileName << " is too long";
        return false;
    }

    if (d->mode == Write) {
        debugStore << "opening for writing" << d->fileName;
        if (d->filesList.contains(d->fileName)) {
            warnStore << "KoStore: Duplicate filename" << d->fileName;
            return false;
        }

        d->filesList.append(d->fileName);

        d->size = 0;
        if (!openWrite(d->fileName)) {
            return false;
        }
    } else if (d->mode == Read) {
        debugStore << "Opening for reading" << d->fileName;
        if (!openRead(d->fileName)) {
            return false;
        }
    } else {
        return false;
    }

    d->isOpen = true;
    return true;
}

bool KoStore::isOpen() const
{
    const KoStorePrivate *d = d_func();
    return d->isOpen;
}

bool KoStore::close()
{
    KoStorePrivate *d = d_func();
    if (!d->isOpen) {
        warnStore << "You must open before closing";
        return false;
    }

    bool ret = d->mode == Write ? closeWrite() : closeRead();
    delete d->stream;
    d->stream = 0;
    d->isOpen = false;
    return ret;
}

PkStream *KoStore::device() const
{
    const KoStorePrivate *d = d_func();
    if (!d->isOpen) {
        warnStore << "You must open before asking for a device";
    }
    if (d->mode != Read) {
        warnStore << "Can not get device from store that is opened for writing";
    }
    return d->stream;
}

PkByteArray KoStore::read(PkStream::pk_int64 max)
{
    KoStorePrivate *d = d_func();
    PkByteArray data;

    if (!d->isOpen) {
        warnStore << "You must open before reading";
        return data;
    }
    if (d->mode != Read) {
        errorStore << "KoStore: Can not read from store that is opened for writing";
        return data;
    }

    if (max == -1) {
        // 后端若在 Read 模式 size 仍为 -1（未知），按 0 处理，避免
        // std::vector<char>((size_t)-1) 巨大分配/抛异常。
        max = (d->size >= 0) ? d->size : 0;
    }
    if (max == 0) {
        return data;
    }

    // PkStream 没有「返回 PkByteArray 的 read(pk_int64)」（readAll() 声明不定义），
    // 这里用 std::vector<char> 中间缓冲转一次再构 PkByteArray（brief §read 契约）。
    std::vector<char> buf(static_cast<std::size_t>(max));
    const PkStream::pk_int64 n = d->stream->read(buf.data(), max);
    // PkStream::read 返回 -1 表示 I/O 错误（未打开/后端失败）。原 Qt 语义返回空
    // 字节数组，这里同样返回空 PkByteArray——不能把 -1 传给 PkByteArray(buf, -1)
    // （std::vector::reserve(-1) 抛 std::length_error）。
    if (n < 0) {
        return data;
    }
    return PkByteArray(buf.data(), static_cast<int>(n));
}

PkStream::pk_int64 KoStore::write(const PkByteArray &data)
{
    return write((const char *)data.data(), data.size());   // see below
}

PkStream::pk_int64 KoStore::read(char *_buffer, PkStream::pk_int64 _len)
{
    KoStorePrivate *d = d_func();
    if (!d->isOpen) {
        errorStore << "KoStore: You must open before reading";
        return -1;
    }
    if (d->mode != Read) {
        errorStore << "KoStore: Can not read from store that is opened for writing";
        return -1;
    }

    return d->stream->read(_buffer, _len);
}

PkStream::pk_int64 KoStore::write(const char *_data, PkStream::pk_int64 _len)
{
    KoStorePrivate *d = d_func();
    if (_len == 0) {
        return 0;
    }

    if (!d->isOpen) {
        errorStore << "KoStore: You must open before writing";
        return 0;
    }
    if (d->mode != Write) {
        errorStore << "KoStore: Can not write to store that is opened for reading";
        return 0;
    }

    PkStream::pk_int64 nwritten = d->stream->write(_data, _len);
    assert(nwritten == _len);
    d->size += nwritten;

    return nwritten;
}

PkStream::pk_int64 KoStore::size() const
{
    const KoStorePrivate *d = d_func();
    if (!d->isOpen) {
        warnStore << "You must open before asking for a size";
        return -1;
    }
    if (d->mode != Read) {
        warnStore << "Can not get size from store that is opened for writing";
        return -1;
    }
    return d->size;
}

bool KoStore::enterDirectory(const PkString &directory)
{
    KoStorePrivate *d = d_func();

    if (directory.isEmpty()) {
        return true;
    }

    const std::vector<PkString> parts = directory.split(u'/');
    for (std::size_t i = 0; i < parts.size(); ++i) {
        // A trailing slash in the input means "enter that directory": the empty
        // final segment is skipped. This matches the original Krita indexOf loop.
        if (parts[i].isEmpty() && i == parts.size() - 1) {
            continue;
        }
        if (!d->enterDirectoryInternal(parts[i])) {
            return false;
        }
    }
    return true;
}

bool KoStore::leaveDirectory()
{
    KoStorePrivate *d = d_func();
    if (d->currentPath.isEmpty()) {
        return false;
    }

    d->currentPath.pop_back();

    return enterAbsoluteDirectory(currentPath());
}

PkString KoStore::currentPath() const
{
    const KoStorePrivate *d = d_func();
    PkString path;
    PkStringList::ConstIterator it = d->currentPath.begin();
    PkStringList::ConstIterator end = d->currentPath.end();
    for (; it != end; ++it) {
        path += *it;
        path += PkString("/");
    }
    return path;
}

void KoStore::pushDirectory()
{
    KoStorePrivate *d = d_func();
    d->directoryStack.push(currentPath());
}

void KoStore::popDirectory()
{
    KoStorePrivate *d = d_func();
    d->currentPath.clear();
    enterAbsoluteDirectory(PkString());
    enterDirectory(d->directoryStack.pop());
}

bool KoStore::extractFile(const PkString &sourceName, PkByteArray &data)
{
    KoStorePrivate *d = d_func();
    PkMemoryStream mem;
    bool ok = d->extractFile(sourceName, mem);
    if (ok) {
        data = PkByteArray(mem.data(), static_cast<int>(mem.size()));
    }
    return ok;
}

bool KoStorePrivate::extractFile(const PkString &sourceName, PkStream &buffer)
{
    if (!q->open(sourceName)) {
        return false;
    }

    if (!buffer.open(PkStream::WriteOnly)) {
        q->close();
        return false;
    }

    std::vector<char> data(8 * 1024);
    PkStream::pk_int64 total = 0;
    for (PkStream::pk_int64 block = 0; (block = q->read(data.data(), static_cast<PkStream::pk_int64>(data.size()))) > 0; total += block) {
        buffer.write(data.data(), block);
    }

    if (q->size() != -1) {
        assert(total == q->size());
    }

    buffer.close();
    q->close();

    return true;
}

bool KoStore::seek(PkStream::pk_int64 pos)
{
    KoStorePrivate *d = d_func();
    if (!d->stream) {
        return false;
    }
    return d->stream->seek(pos);
}

PkStream::pk_int64 KoStore::pos() const
{
    const KoStorePrivate *d = d_func();
    if (!d->stream) {
        return 0;
    }
    return d->stream->pos();
}

bool KoStore::atEnd() const
{
    const KoStorePrivate *d = d_func();
    if (!d->stream) {
        return true;
    }
    return d->stream->atEnd();
}

// See the specification for details of what this function does.
PkString KoStorePrivate::toExternalNaming(const PkString &_internalNaming) const
{
    if (_internalNaming == ROOTPART) {
        return q->currentPath() + MAINNAME;
    }

    PkString intern;
    if (_internalNaming.startsWith("tar:/")) { // absolute reference
        intern = _internalNaming.mid(5);   // remove protocol
    } else {
        intern = q->currentPath() + _internalNaming;
    }

    return intern;
}

bool KoStorePrivate::enterDirectoryInternal(const PkString &directory)
{
    if (q->enterRelativeDirectory(directory)) {
        currentPath.append(directory);
        return true;
    }
    return false;
}

bool KoStore::hasFile(const PkString &fileName) const
{
    const KoStorePrivate *d = d_func();
    return fileExists(d->toExternalNaming(fileName));
}

bool KoStore::hasDirectory(const PkString &directoryName)
{
    return enterAbsoluteDirectory(directoryName);
}

bool KoStore::finalize()
{
    KoStorePrivate *d = d_func();
    assert(!d->finalized);   // call this only once!
    d->finalized = true;
    return doFinalize();
}

void KoStore::setCompressionEnabled(bool e)
{
    (void)e;
}

void KoStore::setSubstitution(const PkString &name, const PkString &substitution)
{
    KoStorePrivate *d = d_func();
    d->substituteThis = name;
    d->substituteWith = substitution;
}

bool KoStore::bad() const
{
    const KoStorePrivate *d = d_func();
    return !d->good;
}

KoStore::Mode KoStore::mode() const
{
    const KoStorePrivate *d = d_func();
    return d->mode;
}

PkStringList KoStore::directoryList() const
{
    return PkStringList();
}
