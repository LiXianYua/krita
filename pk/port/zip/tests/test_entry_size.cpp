// 回归测试：PkZipEntryStream::size()（R-26 Task 2，修 KoStore::size() 恒 0 →
// 锁外几十处 store->read(store->size()) 读空，.kra 加载核心链）。
//
// 独立小 main()（不引入 pk/test harness，避免拉 pk/test 依赖；断言用文件内
// 自带简单宏 VERIFY）。用例：
//   1. 一条已知多字节内容（"Hello, Krita! 你好。"）写进 zip，读模式打开，
//      s->size() == content.size()。
//   2. 空 entry（写 0 字节）读回 size()==0。
//   3. 大 entry（64KB 可压缩内容）读回 size()==65536。
//   4. 读全量 read(size()) 与原始内容逐字节一致。
//   5. 附加：不压缩（stored）的 entry 读回 size() 仍等于原始字节数。
//   6. 顺序条目流的 bytesAvailable() 按底层已消费字节递减，
//      同时计入 unget 缓冲；这是 canReadLine()/atEnd() 的前提。
//   7. Write 模式中间列举条目不得改写中央目录的追加位置。
#include "../PkZipArchive.h"
#include "../../PkStream.h"
#include "PkString.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

namespace {

int g_failures = 0;

#define VERIFY(cond)                                                       \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_failures;                                                  \
        }                                                                  \
    } while (0)

std::string uniqueTempPath(const char *suffix)
{
    return "/tmp/pk_entry_size_test_" + std::to_string(static_cast<long>(::getpid())) + suffix;
}

// 从顺序条目流里读满 n 字节。minizip-ng 的 entry_read 可能短读（解压流按内部
// 缓冲交付），这里循环凑满，不依赖"一次 read(n) 返回 n"。
std::string readN(PkStream *s, PkStream::pk_int64 n)
{
    std::string out(static_cast<std::size_t>(n), '\0');
    PkStream::pk_int64 got = 0;
    while (got < n) {
        const PkStream::pk_int64 r = s->read(&out[static_cast<std::size_t>(got)], n - got);
        if (r <= 0) {
            break;
        }
        got += r;
    }
    out.resize(static_cast<std::size_t>(got));
    return out;
}

void writeZip(const std::string &path,
              const std::vector<std::pair<std::string, std::string>> &entries,
              bool compressionEnabled = true)
{
    PkZipArchive writer(PkZipArchive::Write);
    VERIFY(writer.openFile(PkString(path.c_str())));
    for (const auto &e : entries) {
        PkStream *out = writer.openEntryForWrite(PkString(e.first.c_str()), 0444, compressionEnabled);
        VERIFY(out != nullptr);
        if (out) {
            if (!e.second.empty()) {
                VERIFY(out->write(e.second.data(), static_cast<PkStream::pk_int64>(e.second.size()))
                       == static_cast<PkStream::pk_int64>(e.second.size()));
            }
            delete out;
        }
    }
    VERIFY(writer.close());
}

} // namespace

int main()
{
    // 用例 1+4：一条已知多字节内容，读回 size() == 内容字节数，且
    // read(size()) 全量读回与原始内容逐字节一致。
    {
        const std::string content = "Hello, Krita! 你好。";
        const std::string path = uniqueTempPath("_known.zip");
        writeZip(path, {{"hello.txt", content}});

        PkZipArchive reader(PkZipArchive::Read);
        VERIFY(reader.openFile(PkString(path.c_str())));
        VERIFY(reader.locateEntry(PkString("hello.txt")));
        PkStream *s = reader.openEntryForRead();
        VERIFY(s != nullptr);
        if (s) {
            VERIFY(s->size() == static_cast<PkStream::pk_int64>(content.size()));
            VERIFY(readN(s, s->size()) == content);
            delete s;
        }
        VERIFY(reader.close());
        std::remove(path.c_str());
    }

    // 用例 2：空 entry（写 0 字节）读回 size()==0。
    {
        const std::string path = uniqueTempPath("_empty.zip");
        writeZip(path, {{"empty.txt", ""}});

        PkZipArchive reader(PkZipArchive::Read);
        VERIFY(reader.openFile(PkString(path.c_str())));
        VERIFY(reader.locateEntry(PkString("empty.txt")));
        PkStream *s = reader.openEntryForRead();
        VERIFY(s != nullptr);
        if (s) {
            VERIFY(s->size() == 0);
            VERIFY(readN(s, s->size()) == std::string());
            delete s;
        }
        VERIFY(reader.close());
        std::remove(path.c_str());
    }

    // 用例 3：大 entry（64KB 可压缩内容）读回 size()==65536，内容一致。
    {
        const std::string payload(64 * 1024, 'A');
        const std::string path = uniqueTempPath("_large.zip");
        writeZip(path, {{"payload.bin", payload}});

        PkZipArchive reader(PkZipArchive::Read);
        VERIFY(reader.openFile(PkString(path.c_str())));
        VERIFY(reader.locateEntry(PkString("payload.bin")));
        PkStream *s = reader.openEntryForRead();
        VERIFY(s != nullptr);
        if (s) {
            VERIFY(s->size() == 65536);
            VERIFY(readN(s, s->size()) == payload);
            delete s;
        }
        VERIFY(reader.close());
        std::remove(path.c_str());
    }

    // 用例 5（附加）：不压缩（stored）的 entry，size() 仍等于原始字节数——
    // 压缩与否只影响存储形式，不影响中央目录里 uncompressed_size 的值。
    {
        const std::string payload(64 * 1024, 'B');
        const std::string path = uniqueTempPath("_stored.zip");
        writeZip(path, {{"stored.bin", payload}}, /*compressionEnabled=*/false);

        PkZipArchive reader(PkZipArchive::Read);
        VERIFY(reader.openFile(PkString(path.c_str())));
        VERIFY(reader.locateEntry(PkString("stored.bin")));
        PkStream *s = reader.openEntryForRead();
        VERIFY(s != nullptr);
        if (s) {
            VERIFY(s->size() == static_cast<PkStream::pk_int64>(payload.size()));
            VERIFY(readN(s, s->size()) == payload);
            delete s;
        }
        VERIFY(reader.close());
        std::remove(path.c_str());
    }

    // 用例 6：PkZipEntryStream 是顺序设备，但中央目录已给出
    // uncompressed_size。它必须像 Qt 5.15 中知道底层缓冲量的
    // QIODevice 子类一样，报告「未消费底层字节 + QIODevice 基类
    // unget 缓冲」。否则初始 bytesAvailable()==0 会让 canReadLine()
    // 在读取 KRA tiled-data 头之前就退出。
    {
        const std::string content = "header\npayload";
        const std::string path = uniqueTempPath("_available.zip");
        writeZip(path, {{"stream.bin", content}});

        PkZipArchive reader(PkZipArchive::Read);
        VERIFY(reader.openFile(PkString(path.c_str())));
        VERIFY(reader.locateEntry(PkString("stream.bin")));
        PkStream *s = reader.openEntryForRead();
        VERIFY(s != nullptr);
        if (s) {
            const PkStream::pk_int64 total = static_cast<PkStream::pk_int64>(content.size());
            VERIFY(s->bytesAvailable() == total);
            VERIFY(s->canReadLine());
            VERIFY(!s->atEnd());

            char prefix[3] = {};
            VERIFY(s->read(prefix, 3) == 3);
            VERIFY(std::string(prefix, 3) == "hea");
            VERIFY(s->bytesAvailable() == total - 3);

            s->ungetChar('a');
            VERIFY(s->bytesAvailable() == total - 2);
            char replay = '\0';
            VERIFY(s->getChar(&replay));
            VERIFY(replay == 'a');
            VERIFY(s->bytesAvailable() == total - 3);

            VERIFY(readN(s, total) == content.substr(3));
            VERIFY(s->bytesAvailable() == 0);
            VERIFY(!s->canReadLine());
            VERIFY(s->atEnd());
            delete s;
        }
        VERIFY(reader.close());
        std::remove(path.c_str());
    }

    // 用例 7：KoQuaZipStore 会在写一个图层目录后列举已有条目，
    // 再继续写后续条目。entryNames() 必须保留 minizip-ng 内存中央
    // 目录流的追加位置；否则后续条目会从首条末尾开始覆盖。
    {
        const std::string path = uniqueTempPath("_list_while_writing.zip");
        PkZipArchive writer(PkZipArchive::Write);
        VERIFY(writer.openFile(PkString(path.c_str())));

        const auto writeEntry = [&writer](const char *name, const char *content) {
            PkStream *out = writer.openEntryForWrite(PkString(name), 0444, true);
            VERIFY(out != nullptr);
            if (out) {
                const auto size = static_cast<PkStream::pk_int64>(std::strlen(content));
                VERIFY(out->write(content, size) == size);
                delete out;
            }
        };

        writeEntry("first.txt", "first");
        writeEntry("before-list.txt", "before");
        const std::vector<PkString> namesDuringWrite = writer.entryNames();
        VERIFY(namesDuringWrite.size() == 2);
        writeEntry("after-list.txt", "after");
        VERIFY(writer.close());

        PkZipArchive reader(PkZipArchive::Read);
        VERIFY(reader.openFile(PkString(path.c_str())));
        const std::vector<PkString> finalNames = reader.entryNames();
        VERIFY(finalNames.size() == 3);
        VERIFY(reader.locateEntry(PkString("first.txt")));
        VERIFY(reader.locateEntry(PkString("before-list.txt")));
        VERIFY(reader.locateEntry(PkString("after-list.txt")));
        VERIFY(reader.close());
        std::remove(path.c_str());
    }

    if (g_failures != 0) {
        std::fprintf(stderr, "test_entry_size: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_entry_size: all pass\n");
    return 0;
}
