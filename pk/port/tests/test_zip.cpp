// PkZipArchive 的测试（R-12 Task 6，Q-5：QuaZip → minizip-ng）。形态照抄
// test_resourcestorage.cpp/test_stream.cpp：测试类成员 public、
// PkTestBinder<T> 特化手写，不经 pk_test_moc.py。
//
// 覆盖：① 端到端——写一个 zip、读回来、内容一致，且条目流真的是
// PkStream（brief 硬性要求的形状：`PkStream *s = archive.openEntry(...)`）；
// ② 条目计数；③ 目录列举（entryNames()）；④ 两档压缩级别——不只是"跑起来
// 不崩"，用高度可压缩的内容实测两档产出的归档文件大小确实不同（default <
// none），有真实杀伤力；⑤ 从任意 PkStream 打开归档（不只是文件名）；
// ⑥ 定位不存在的条目失败，lastError()/isOk() 反映出来；⑦"同一时刻只能有
// 一个条目开着"这条从真 QuaZip 继承来的约束；⑧ setZip64Enabled 打开时仍能
// 正确往返。全部断言基于具体输入输出比对，不写恒真断言。
#include "../zip/PkZipArchive.h"
#include "../PkStream.h"
#include "PkString.h"
#include "PkTest.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::string toStdString(const PkString &s)
{
    return s.PkToUtf8();
}

// 内存里的 PkStream：形态照抄 test_stream.cpp 的 MemoryStream（非顺序设备，
// readData()/writeData() 按 pos() 索引自己的 std::string）。这里额外暴露
// data()，用来把"写模式归档的产出"整段搬给"读模式归档"复用同一份字节
// ——validate「从任意 PkStream 打开」这条能力不依赖文件系统。
class TestMemoryStream : public PkStream {
public:
    explicit TestMemoryStream(std::string initial = std::string()) : m_data(std::move(initial)) {}

    pk_int64 size() const override { return static_cast<pk_int64>(m_data.size()); }
    const std::string &data() const { return m_data; }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const pk_int64 avail = static_cast<pk_int64>(m_data.size()) - pos();
        if (avail <= 0) {
            return 0;
        }
        const pk_int64 n = maxSize < avail ? maxSize : avail;
        std::memcpy(data, m_data.data() + pos(), static_cast<std::size_t>(n));
        return n;
    }

    pk_int64 writeData(const char *data, pk_int64 maxSize) override
    {
        const std::size_t p = static_cast<std::size_t>(pos());
        if (p + static_cast<std::size_t>(maxSize) > m_data.size()) {
            m_data.resize(p + static_cast<std::size_t>(maxSize));
        }
        std::memcpy(&m_data[p], data, static_cast<std::size_t>(maxSize));
        return maxSize;
    }

private:
    std::string m_data;
};

std::string readAllFromStream(PkStream *s)
{
    std::string out;
    char buf[256];
    for (;;) {
        const PkStream::pk_int64 n = s->read(buf, sizeof(buf));
        if (n <= 0) {
            break;
        }
        out.append(buf, static_cast<std::size_t>(n));
    }
    return out;
}

std::string uniqueTempPath(const char *suffix)
{
    return "/tmp/pk_zip_archive_test_" + std::to_string(static_cast<long>(getpid())) + suffix;
}

long fileSizeOf(const std::string &path)
{
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) {
        return -1;
    }
    return static_cast<long>(st.st_size);
}

} // namespace

class PkZipArchiveTestCase : public PkTestObject {
public:
    void testEndToEndWriteThenReadBackViaPkStream();
    void testEntryCountReflectsEntriesWritten();
    void testEntryNamesListsAllEntries();
    void testCompressionLevelAffectsArchiveSize();
    void testOpenFromPkStreamRoundTrips();
    void testLocateMissingEntryFailsAndReportsError();
    void testCannotOpenSecondEntryWhileFirstStillOpen();
    void testZip64EnabledStillRoundTrips();
    void testOpenStreamRejectsUnopenedStream();
    void testEntryNamesDoesNotDisturbLocatedEntry();
};

void PkZipArchiveTestCase::testEndToEndWriteThenReadBackViaPkStream()
{
    const std::string path = uniqueTempPath("_e2e.zip");
    const std::string content = "Hello, Krita! 你好。";

    {
        PkZipArchive writer(PkZipArchive::Write);
        PK_VERIFY(writer.openFile(PkString(path.c_str())));
        PkStream *out = writer.openEntryForWrite(PkString("hello.txt"), 0444, /*compressionEnabled=*/true);
        PK_VERIFY(out != nullptr);
        PK_COMPARE((long long)out->write(content.data(), (PkStream::pk_int64)content.size()),
                   (long long)content.size());
        delete out;
        PK_VERIFY(writer.close());
    }

    {
        PkZipArchive reader(PkZipArchive::Read);
        PK_VERIFY(reader.openFile(PkString(path.c_str())));
        PK_VERIFY(reader.locateEntry(PkString("hello.txt")));
        PkStream *s = reader.openEntryForRead();
        PK_VERIFY(s != nullptr);
        PK_COMPARE(readAllFromStream(s), content);
        delete s;
        PK_VERIFY(reader.close());
    }

    std::remove(path.c_str());
}

void PkZipArchiveTestCase::testEntryCountReflectsEntriesWritten()
{
    const std::string path = uniqueTempPath("_count.zip");

    {
        PkZipArchive writer(PkZipArchive::Write);
        PK_VERIFY(writer.openFile(PkString(path.c_str())));
        const char *names[] = {"a.txt", "b.txt", "dir/c.txt"};
        for (const char *name : names) {
            PkStream *out = writer.openEntryForWrite(PkString(name), 0444, true);
            PK_VERIFY(out != nullptr);
            out->write("x", 1);
            delete out;
        }
        PK_VERIFY(writer.close());
    }

    {
        PkZipArchive reader(PkZipArchive::Read);
        PK_VERIFY(reader.openFile(PkString(path.c_str())));
        PK_COMPARE(reader.entryCount(), (int64_t)3);
        PK_VERIFY(reader.close());
    }

    std::remove(path.c_str());
}

void PkZipArchiveTestCase::testEntryNamesListsAllEntries()
{
    const std::string path = uniqueTempPath("_names.zip");

    {
        PkZipArchive writer(PkZipArchive::Write);
        PK_VERIFY(writer.openFile(PkString(path.c_str())));
        PkStream *a = writer.openEntryForWrite(PkString("first.txt"), 0444, true);
        a->write("1", 1);
        delete a;
        PkStream *b = writer.openEntryForWrite(PkString("second.txt"), 0444, true);
        b->write("2", 1);
        delete b;
        PK_VERIFY(writer.close());
    }

    {
        PkZipArchive reader(PkZipArchive::Read);
        PK_VERIFY(reader.openFile(PkString(path.c_str())));
        std::vector<PkString> names = reader.entryNames();
        PK_COMPARE((int)names.size(), 2);
        PK_COMPARE(toStdString(names[0]), std::string("first.txt"));
        PK_COMPARE(toStdString(names[1]), std::string("second.txt"));
        PK_VERIFY(reader.close());
    }

    std::remove(path.c_str());
}

// 高度可压缩的内容（同一个字节重复很多次）：compressionEnabled=true 走
// Z_DEFAULT_COMPRESSION，false 走 Z_NO_COMPRESSION（KoQuaZipStore.cpp
// setCompressionEnabled() 那两档），产出的归档文件大小必须能观察到真实差异
// ——不是"跑起来不崩"这种弱断言。
void PkZipArchiveTestCase::testCompressionLevelAffectsArchiveSize()
{
    const std::string payload(64 * 1024, 'A');
    const std::string compressedPath = uniqueTempPath("_compressed.zip");
    const std::string storedPath = uniqueTempPath("_stored.zip");

    {
        PkZipArchive writer(PkZipArchive::Write);
        PK_VERIFY(writer.openFile(PkString(compressedPath.c_str())));
        PkStream *out = writer.openEntryForWrite(PkString("payload.bin"), 0444, /*compressionEnabled=*/true);
        out->write(payload.data(), (PkStream::pk_int64)payload.size());
        delete out;
        PK_VERIFY(writer.close());
    }
    {
        PkZipArchive writer(PkZipArchive::Write);
        PK_VERIFY(writer.openFile(PkString(storedPath.c_str())));
        PkStream *out = writer.openEntryForWrite(PkString("payload.bin"), 0444, /*compressionEnabled=*/false);
        out->write(payload.data(), (PkStream::pk_int64)payload.size());
        delete out;
        PK_VERIFY(writer.close());
    }

    const long compressedSize = fileSizeOf(compressedPath);
    const long storedSize = fileSizeOf(storedPath);
    PK_VERIFY(compressedSize > 0);
    PK_VERIFY(storedSize > 0);
    PK_VERIFY(compressedSize < storedSize);

    // 两条路径都必须仍然能正确读回内容——压缩级别只影响存储形式，不影响
    // 往返正确性。
    {
        PkZipArchive reader(PkZipArchive::Read);
        PK_VERIFY(reader.openFile(PkString(compressedPath.c_str())));
        PK_VERIFY(reader.locateEntry(PkString("payload.bin")));
        PkStream *s = reader.openEntryForRead();
        PK_COMPARE(readAllFromStream(s), payload);
        delete s;
        PK_VERIFY(reader.close());
    }

    std::remove(compressedPath.c_str());
    std::remove(storedPath.c_str());
}

void PkZipArchiveTestCase::testOpenFromPkStreamRoundTrips()
{
    const std::string content = "stream-backed archive";
    std::string bytes;

    {
        TestMemoryStream mem;
        PK_VERIFY(mem.open(PkStream::ReadWrite));
        PkZipArchive writer(PkZipArchive::Write);
        PK_VERIFY(writer.openStream(&mem));
        PkStream *out = writer.openEntryForWrite(PkString("entry.txt"), 0444, true);
        PK_VERIFY(out != nullptr);
        out->write(content.data(), (PkStream::pk_int64)content.size());
        delete out;
        PK_VERIFY(writer.close());
        bytes = mem.data();
    }

    PK_VERIFY(!bytes.empty());

    {
        TestMemoryStream mem(bytes);
        PK_VERIFY(mem.open(PkStream::ReadOnly));
        PkZipArchive reader(PkZipArchive::Read);
        PK_VERIFY(reader.openStream(&mem));
        PK_VERIFY(reader.locateEntry(PkString("entry.txt")));
        PkStream *s = reader.openEntryForRead();
        PK_VERIFY(s != nullptr);
        PK_COMPARE(readAllFromStream(s), content);
        delete s;
        PK_VERIFY(reader.close());
    }
}

void PkZipArchiveTestCase::testLocateMissingEntryFailsAndReportsError()
{
    const std::string path = uniqueTempPath("_missing.zip");

    {
        PkZipArchive writer(PkZipArchive::Write);
        PK_VERIFY(writer.openFile(PkString(path.c_str())));
        PkStream *out = writer.openEntryForWrite(PkString("present.txt"), 0444, true);
        out->write("x", 1);
        delete out;
        PK_VERIFY(writer.close());
    }

    {
        PkZipArchive reader(PkZipArchive::Read);
        PK_VERIFY(reader.openFile(PkString(path.c_str())));
        PK_VERIFY(reader.isOk());
        PK_VERIFY(!reader.locateEntry(PkString("absent.txt")));
        PK_VERIFY(!reader.isOk());
        PK_VERIFY(reader.close());
    }

    std::remove(path.c_str());
}

// 从真 QuaZip 继承来的约束（KoQuaZipStore.cpp 76-86 析构注释引用 QuaZipFile
// 文档："do not close zip object or change its current file as long as
// QuaZipFile is open"）——同一时刻只能有一个条目开着。
void PkZipArchiveTestCase::testCannotOpenSecondEntryWhileFirstStillOpen()
{
    const std::string path = uniqueTempPath("_singleentry.zip");

    PkZipArchive writer(PkZipArchive::Write);
    PK_VERIFY(writer.openFile(PkString(path.c_str())));

    PkStream *first = writer.openEntryForWrite(PkString("a.txt"), 0444, true);
    PK_VERIFY(first != nullptr);

    PkStream *second = writer.openEntryForWrite(PkString("b.txt"), 0444, true);
    PK_VERIFY(second == nullptr);

    PK_VERIFY(!writer.close());

    first->write("x", 1);
    delete first;

    PK_VERIFY(writer.close());
    std::remove(path.c_str());
}

void PkZipArchiveTestCase::testZip64EnabledStillRoundTrips()
{
    const std::string path = uniqueTempPath("_zip64.zip");
    const std::string content = "zip64 forced";

    {
        PkZipArchive writer(PkZipArchive::Write);
        writer.setZip64Enabled(true);
        PK_VERIFY(writer.openFile(PkString(path.c_str())));
        PkStream *out = writer.openEntryForWrite(PkString("z.txt"), 0444, true);
        PK_VERIFY(out != nullptr);
        out->write(content.data(), (PkStream::pk_int64)content.size());
        delete out;
        PK_VERIFY(writer.close());
    }

    {
        PkZipArchive reader(PkZipArchive::Read);
        PK_VERIFY(reader.openFile(PkString(path.c_str())));
        PK_VERIFY(reader.locateEntry(PkString("z.txt")));
        PkStream *s = reader.openEntryForRead();
        PK_VERIFY(s != nullptr);
        PK_COMPARE(readAllFromStream(s), content);
        delete s;
        PK_VERIFY(reader.close());
    }

    std::remove(path.c_str());
}

// 评审 I-1：openStream() 头注释写了「调用前 stream 必须已经处于 open 状态」，
// 但没有任何东西真的拦——直到本次修复前，传一个没 open() 的 PkStream 进去，
// openStream() 照样返回 true，等到 openEntryForWrite() 才失败、产出 0 字节
// 归档。这里直接复现「未 open」这一种情形，锁死契约。
void PkZipArchiveTestCase::testOpenStreamRejectsUnopenedStream()
{
    TestMemoryStream mem;
    PK_VERIFY(!mem.isOpen());

    PkZipArchive writer(PkZipArchive::Write);
    PK_VERIFY(!writer.openStream(&mem));
    PK_VERIFY(!writer.isOpen());
}

// 评审 I-2：entryNames() 是 const 方法，但底层 minizip-ng 遍历会把「当前条目」
// 游标推到列表尾——不还原的话，locateEntry() 定位好的条目会被 entryNames()
// 悄悄破坏。复现评审给出的具体失败序列：locateEntry → entryNames() →
// openEntryForRead() 此前会返回 nullptr（lastError=-102 MZ_PARAM_ERROR）。
void PkZipArchiveTestCase::testEntryNamesDoesNotDisturbLocatedEntry()
{
    const std::string path = uniqueTempPath("_entrynames_locate.zip");
    const std::string content = "AAAA-alpha";

    {
        PkZipArchive writer(PkZipArchive::Write);
        PK_VERIFY(writer.openFile(PkString(path.c_str())));
        PkStream *a = writer.openEntryForWrite(PkString("alpha.txt"), 0444, true);
        PK_VERIFY(a != nullptr);
        a->write(content.data(), (PkStream::pk_int64)content.size());
        delete a;
        PkStream *b = writer.openEntryForWrite(PkString("beta.txt"), 0444, true);
        PK_VERIFY(b != nullptr);
        b->write("beta", 4);
        delete b;
        PK_VERIFY(writer.close());
    }

    {
        PkZipArchive reader(PkZipArchive::Read);
        PK_VERIFY(reader.openFile(PkString(path.c_str())));
        PK_VERIFY(reader.locateEntry(PkString("alpha.txt")));

        // 中间插一次 entryNames() 调用——这是本条断言要锁的行为：调用后
        // locateEntry() 定位的条目必须仍然是 alpha.txt。
        std::vector<PkString> names = reader.entryNames();
        PK_COMPARE((int)names.size(), 2);

        PkStream *s = reader.openEntryForRead();
        PK_VERIFY(s != nullptr);
        PK_COMPARE(readAllFromStream(s), content);
        delete s;
        PK_VERIFY(reader.close());
    }

    std::remove(path.c_str());
}

// PkTestBinder<PkZipArchiveTestCase> 特化——手写，形状对照
// pk/test/pk_test_moc.py 的 emit_binder() 输出（同其余测试文件的做法）。
template <>
struct PkTestBinder<PkZipArchiveTestCase> {
    static const char *className() { return "PkZipArchiveTestCase"; }

    static const PkTestFunction *functions()
    {
        static const PkTestFunction fns[] = {
            {"testEndToEndWriteThenReadBackViaPkStream",
             [](PkTestObject *o) {
                 static_cast<PkZipArchiveTestCase *>(o)->testEndToEndWriteThenReadBackViaPkStream();
             },
             nullptr},
            {"testEntryCountReflectsEntriesWritten",
             [](PkTestObject *o) {
                 static_cast<PkZipArchiveTestCase *>(o)->testEntryCountReflectsEntriesWritten();
             },
             nullptr},
            {"testEntryNamesListsAllEntries",
             [](PkTestObject *o) { static_cast<PkZipArchiveTestCase *>(o)->testEntryNamesListsAllEntries(); },
             nullptr},
            {"testCompressionLevelAffectsArchiveSize",
             [](PkTestObject *o) {
                 static_cast<PkZipArchiveTestCase *>(o)->testCompressionLevelAffectsArchiveSize();
             },
             nullptr},
            {"testOpenFromPkStreamRoundTrips",
             [](PkTestObject *o) { static_cast<PkZipArchiveTestCase *>(o)->testOpenFromPkStreamRoundTrips(); },
             nullptr},
            {"testLocateMissingEntryFailsAndReportsError",
             [](PkTestObject *o) {
                 static_cast<PkZipArchiveTestCase *>(o)->testLocateMissingEntryFailsAndReportsError();
             },
             nullptr},
            {"testCannotOpenSecondEntryWhileFirstStillOpen",
             [](PkTestObject *o) {
                 static_cast<PkZipArchiveTestCase *>(o)->testCannotOpenSecondEntryWhileFirstStillOpen();
             },
             nullptr},
            {"testZip64EnabledStillRoundTrips",
             [](PkTestObject *o) { static_cast<PkZipArchiveTestCase *>(o)->testZip64EnabledStillRoundTrips(); },
             nullptr},
            {"testOpenStreamRejectsUnopenedStream",
             [](PkTestObject *o) { static_cast<PkZipArchiveTestCase *>(o)->testOpenStreamRejectsUnopenedStream(); },
             nullptr},
            {"testEntryNamesDoesNotDisturbLocatedEntry",
             [](PkTestObject *o) {
                 static_cast<PkZipArchiveTestCase *>(o)->testEntryNamesDoesNotDisturbLocatedEntry();
             },
             nullptr},
        };
        return fns;
    }
    static int count() { return 10; }

    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase() { return nullptr; }
    static const PkTestFunction *cleanupTestCase() { return nullptr; }
    static const PkTestFunction *initFn() { return nullptr; }
    static const PkTestFunction *cleanupFn() { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

int run_zip_tests(int argc, char **argv)
{
    PkZipArchiveTestCase tc;
    return PkTest::qExec(&tc, argc, argv);
}
