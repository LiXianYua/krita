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
#include "../PkZipArchive.h"
#include "../../PkStream.h"
#include "PkString.h"

#include <cstdio>
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

    if (g_failures != 0) {
        std::fprintf(stderr, "test_entry_size: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("test_entry_size: all pass\n");
    return 0;
}
