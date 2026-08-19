/*
 * SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KoQuaZipStore.h"
#include "KoStore_p.h"

#include <StoreDebug.h>

#include "PkZipArchive.h"
#include "PkMemoryStream.h"
#include "PkConfigGroup.h"
#include "PkSharedConfig.h"

#include <cstring>
#include <memory>
#include <vector>

namespace {

// PkString 没有 replace：手写折叠连续 '/'（fixedPath.replace("//", "/") 的对应物）。
// "a//b" → "a/b"，"///" → "/"（与 Qt 的逐次非重叠替换语义略有出入，但意图一致——
// zip 条目路径里连续斜杠折叠成单个）。
PkString collapseSlashes(const PkString &path)
{
    PkString result;
    bool prevSlash = false;
    for (int i = 0; i < path.size(); ++i) {
        if (path.at(i) == u'/') {
            if (!prevSlash) {
                result += path.mid(i, 1);
            }
            prevSlash = true;
        } else {
            result += path.mid(i, 1);
            prevSlash = false;
        }
    }
    return result;
}

// PkString 没有 replace/indexOf：手写 find-and-splice 子串替换
// （fixedPath.replace(substituteThis, substituteWith) 的对应物）。替换全部出现。
PkString replaceAll(const PkString &input, const PkString &from, const PkString &to)
{
    const int n = input.size();
    const int m = from.size();
    if (m == 0) {
        return input;
    }
    if (m > n) {
        return input;
    }
    PkString result;
    int i = 0;
    int runStart = 0;
    while (i + m <= n) {
        bool matched = true;
        for (int j = 0; j < m; ++j) {
            if (input.at(i + j) != from.at(j)) {
                matched = false;
                break;
            }
        }
        if (matched) {
            if (i > runStart) {
                result += input.mid(runStart, i - runStart);
            }
            result += to;
            i += m;
            runStart = i;
        } else {
            ++i;
        }
    }
    if (runStart < n) {
        result += input.mid(runStart, n - runStart);
    }
    return result;
}

// PkZipArchive::entryNames() 返回 std::vector<PkString>，这里转成 PkStringList。
PkStringList toPkStringList(const std::vector<PkString> &names)
{
    PkStringList out;
    for (const PkString &n : names) {
        out.append(n);
    }
    return out;
}

} // namespace

struct KoQuaZipStore::Private {

    Private() {}
    ~Private() {}

    PkZipArchive *archive {nullptr};
    PkStream *currentFile {nullptr};
    PkStringList directoryListCache;
    bool directoryListCached {false};
    bool compressionEnabled {true};   // 原 Z_DEFAULT_COMPRESSION/Z_NO_COMPRESSION 两档 → bool
    PkMemoryStream buffer;
};


KoQuaZipStore::KoQuaZipStore(const PkString &_filename, KoStore::Mode _mode, const PkByteArray &appIdentification, bool writeMimetype)
    : KoStore(_mode, writeMimetype)
    , dd(new Private())
{
    KoStorePrivate *d = d_func();
    d->localFileName = _filename;
    dd->archive = new PkZipArchive(_mode == KoStore::Write ? PkZipArchive::Write : PkZipArchive::Read);
    if (!dd->archive->openFile(_filename)) {
        d->good = false;
        return;
    }
    init(appIdentification);

}

KoQuaZipStore::KoQuaZipStore(PkStream *dev, KoStore::Mode _mode, const PkByteArray &appIdentification, bool writeMimetype)
    : KoStore(_mode, writeMimetype)
    , dd(new Private())
{
    KoStorePrivate *d = d_func();
    dd->archive = new PkZipArchive(_mode == KoStore::Write ? PkZipArchive::Write : PkZipArchive::Read);
    // PkZipArchive::openStream 不替传入的 stream 调 open()（minizip-ng 只 read/write/
    // seek/tell 它），调用前必须已 open。
    if (!dev->open(_mode == KoStore::Write ? PkStream::WriteOnly : PkStream::ReadOnly)) {
        d->good = false;
        return;
    }
    if (!dd->archive->openStream(dev)) {
        d->good = false;
        return;
    }
    init(appIdentification);
}

KoQuaZipStore::~KoQuaZipStore()
{
    KoStorePrivate *d = d_func();

    if (d->good && dd->currentFile && dd->currentFile->isOpen()) {
        dd->currentFile->close();
    }

    if (!d->finalized) {
        finalize();
    }


    // NOTE:
    //   If the dd->currentFile is corrupt (and ->getZipError() returns an error code)
    // QuaZip cannot really close it properly (and I don't see an accessible function to reset the error code).
    // Therefore the destructor thinks the file is open and tries to close it.
    // And closing the file means checking if the zip archive associated with the file is open or not.
    // Therefore the archive must exist when the file is being closed, therefore also when it's being deleted.
    // In comparison, the archive can be deleted whenever and it doesn't check the current file.
    // Therefore we gotta delete the file first, then the zip archive.

    //   From QuaZip code comments for the QuaZipFile constructor:
    // "* Summary: do not close \c zip object or change its current file as
    // * long as QuaZipFile is open."

    if (dd->currentFile) {
        delete dd->currentFile;
    }
    delete dd->archive;

}

void KoQuaZipStore::setCompressionEnabled(bool enabled)
{
    dd->compressionEnabled = enabled;
}

PkStream::pk_int64 KoQuaZipStore::write(const char *_data, PkStream::pk_int64 _len)
{
    KoStorePrivate *d = d_func();
    if (_len == 0) return 0;

    if (!d->isOpen) {
        errorStore << "KoStore: You must open before writing";
        return 0;
    }

    if (d->mode != Write) {
        errorStore << "KoStore: Can not write to store that is opened for reading";
        return 0;
    }

    PkStream::pk_int64 nwritten = dd->buffer.write(_data, _len);
    d->size += nwritten;
    return nwritten;
}

PkStringList KoQuaZipStore::directoryList() const
{
    // If in Read mode, we can assume the directory listing won't change between invocations.
    if (mode() == Read) {
        if (!dd->directoryListCached) {
            dd->directoryListCache = toPkStringList(dd->archive->entryNames());
            dd->directoryListCached = true;
        }
        return dd->directoryListCache;
    }
    else {
        return toPkStringList(dd->archive->entryNames());
    }
}

void KoQuaZipStore::init(const PkByteArray &appIdentification)
{
    KoStorePrivate *d = d_func();

    bool enableZip64 = false;
    // 实测 "application/x-krita" 是 19 字节（off-by-one 防呆：brief 草稿写 20）。
    if (appIdentification.size() == 19 && std::memcmp(appIdentification.data(), "application/x-krita", 19) == 0) {
        enableZip64 = PkSharedConfig::openConfig()->group("").readEntry<bool>("UseZip64", false);
    }

    dd->archive->setDataDescriptorWritingEnabled(false);
    dd->archive->setZip64Enabled(enableZip64);
    dd->archive->setAutoClose(true);

    // openFile/openStream 已在构造里按 Mode 开好（PkZipArchive 构造即定 Mode，
    // openFile 内部用 MZ_OPEN_MODE_READ 或 MZ_OPEN_MODE_WRITE|CREATE）；这里不再
    // 有 archive->open()，good 由构造函数里的 openFile/openStream 结果决定。
    d->good = true;

    if (d->mode == Write) {
        if (d->writeMimetype) {
            PkStream *ms = dd->archive->openEntryForWrite("mimetype", 0444, false);
            if (!ms) {
                d->good = false;
                return;
            }
            ms->write(reinterpret_cast<const char *>(appIdentification.data()), appIdentification.size());
            ms->close();
            delete ms;
        }
    }
    else {
        const int64_t count = dd->archive->entryCount();
        debugStore << count << directoryList();
    }
}

bool KoQuaZipStore::doFinalize()
{
    KoStorePrivate *d = d_func();

    d->stream = 0;
    if (d->good) {
        dd->archive->close();
    }
    return dd->archive->lastError() == 0;

}

bool KoQuaZipStore::openWrite(const PkString &name)
{
    KoStorePrivate *d = d_func();
    PkString fixedPath = collapseSlashes(name);

    delete d->stream;
    d->stream = 0; // Not used when writing

    delete dd->currentFile;
    dd->currentFile = nullptr;

    dd->currentFile = dd->archive->openEntryForWrite(fixedPath, 0444, dd->compressionEnabled);
    if (!dd->currentFile) {
        qWarning() << "Could not open" << name << dd->archive->lastError();
        return false;
    }

    dd->buffer = PkMemoryStream();
    dd->buffer.open(PkStream::WriteOnly);

    return true;
}

bool KoQuaZipStore::openRead(const PkString &name)
{
    KoStorePrivate *d = d_func();

    PkString fixedPath = collapseSlashes(name);

    delete d->stream;
    d->stream = 0;
    delete dd->currentFile;
    dd->currentFile = nullptr;

    if (!currentPath().isEmpty() && !fixedPath.startsWith(currentPath())) {
        fixedPath = currentPath() + PkString("/") + fixedPath;
    }

    if (!d->substituteThis.isEmpty()) {
        fixedPath = replaceAll(fixedPath, d->substituteThis, d->substituteWith);
    }

    if (!dd->archive->locateEntry(fixedPath)) {
        qWarning() << "\t\tCould not set current file" << dd->archive->lastError() << fixedPath;
        return false;
    }

    dd->currentFile = dd->archive->openEntryForRead();
    if (!dd->currentFile) {
        qWarning() << "\t\t\tBut could not open!!!" << dd->archive->lastError();
        return false;
    }
    d->stream = dd->currentFile;
    d->size = dd->currentFile->size();
    return true;
}

bool KoQuaZipStore::closeWrite()
{
    KoStorePrivate *d = d_func();

    bool r = true;
    // PkMemoryStream 空缓冲时 data() 可能为 nullptr，短路跳过写。
    if (dd->buffer.size() > 0) {
        const PkStream::pk_int64 n = dd->currentFile->write(dd->buffer.data(), dd->buffer.size());
        if (n != dd->buffer.size()) {
            // write() returns number of bytes written, or -1 in case of error
            // let's allow write 0 bytes in the cache, when needed
            qWarning() << "Could not write buffer to the file";
            r = false;
        }
    }
    dd->buffer.close();
    dd->currentFile->close();
    delete dd->currentFile;
    dd->currentFile = nullptr;
    d->stream = 0;
    return (r && dd->archive->lastError() == 0);
}

bool KoQuaZipStore::closeRead()
{
    KoStorePrivate *d = d_func();
    d->stream = 0;
    return true;
}

bool KoQuaZipStore::enterRelativeDirectory(const PkString & /*path*/)
{
    return true;
}

bool KoQuaZipStore::enterAbsoluteDirectory(const PkString &path)
{
    PkString fixedPath = collapseSlashes(path);

    if (fixedPath.isEmpty()) {
        fixedPath = "/";
    }

    // 尾无 '/' 时拼 '/' 做目录前缀匹配；PkString 无 endsWith，用 right(1) 判尾斜杠。
    const PkString prefix = (fixedPath.right(1) == PkString("/")) ? fixedPath : fixedPath + PkString("/");

    const std::vector<PkString> names = dd->archive->entryNames();
    for (const PkString &n : names) {
        if (n == fixedPath || n.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

bool KoQuaZipStore::fileExists(const PkString &absPath) const
{
    const KoStorePrivate *d = d_func();

    PkString fixedPath = collapseSlashes(absPath);

    if (!d->substituteThis.isEmpty()) {
        fixedPath = replaceAll(fixedPath, d->substituteThis, d->substituteWith);
    }

    return directoryList().contains(fixedPath);
}
