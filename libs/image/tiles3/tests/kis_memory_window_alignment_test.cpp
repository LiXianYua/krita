/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ⚠ 这不是 libs/image/tiles3/tests/kis_memory_window_test.cpp，是**复刻调用点形状的
 *   driver**（R 线 spec「依赖墙挡住真实测试类时：允许降级成复刻调用点形状的 driver」
 *   这条降级路径）。
 *
 * 为什么不能直接编那个真实测试类：`kis_memory_window_test.cpp` 用 `QString` 构造
 * `KisMemoryWindow`，而 `KisMemoryWindow` 的形参在剥 Qt 时已经变成 `PkString`：
 *
 *     kis_memory_window_test.cpp:19:21: error: no matching constructor for
 *         initialization of 'KisMemoryWindow'
 *         KisMemoryWindow memory(swapDir.path(), 1024);
 *     kis_memory_window.h:28:5: note: candidate constructor not viable: no known
 *         conversion from 'QString' to 'const PkString' for 1st argument
 *
 * 拆掉这堵墙是 **R-65** 的活（`docs/TASKS.md` R-65：把 178 个测试 target 从
 * `kritatestsdk` 迁到 pk 测试栈，状态 `NOT_STARTED`；M5 口径「测试侧 163 个 target
 * 编不过」）。R-65 做完之后，理论上可以补一次「编译并把真实测试类跑绿」；本 driver
 * 本身已被接受为判据② 这一层的证据。
 *
 * 这个 driver 守的是 R-67 修的那条缺陷：**`KisMemoryWindow` 的映射窗口必须页对齐**。
 * `::mmap()` 只接受页大小的整数倍作为文件偏移，非对齐偏移返回 EINVAL；而剥离期把
 * Qt 的 `QFile::map()` 换成裸 `::mmap()` 之后，`KisMemoryWindow::adjustWindow()` 里
 * 原本由 Qt 内部完成的对齐没有了 —— 于是任何不是页对齐的 chunk 偏移都让
 * `mapFile()` 返回 nullptr，`KisSwappedDataStore::swapInTileData()` 再把这个空指针
 * 交给 `KisTileCompressor2::decompressTileData()`，在 `buffer[0]` 上段错误
 * （`PK_TILES_ASSERT(ptr)` 在 `NDEBUG` 下展开成 `((void)sizeof(ptr))`，拦不住）。
 *
 * **零 Qt 头、零 Qt 链接**：plain `main()`，失败数自算，形态照
 * `libs/impex/tests/KisPngCodecNoResourceDirsTest.cpp`（R-59 交付的同一个形态）。
 *
 * 期望值的来源（spec 降级路径第 2 条「校验值来自对真 Qt 的实测探针」）——
 * 探针源码 `.superpowers/sdd/R-67/probe/qt_map_probe.cpp`，链接
 * `krita-ci-env/_install` 里的 QtCore.framework，实测输出：
 *
 *     QFile::map(0, 1048576)    -> 0x105a40000 OK
 *     QFile::map(584, 262144)   -> 0x105b40248 OK     (返回指针 = 映射基址 + 584)
 *     QFile::map(16384, 262144) -> 0x105b84000 OK
 *
 * 即：真 Qt 侧对齐是**成功且透明**的（返回指针已经把 delta 折进去），
 * 所以「写进去什么、读回来必须逐字节相同」这条期望值与 Qt 版一致。
 */

#include "../swap/kis_memory_window.h"
#include "../swap/kis_chunk_allocator.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>

namespace
{

struct PkChunkOffsetCase {
    PkTilesQuint64 offset;
    unsigned char value;
};

int g_failures = 0;

void check(bool ok, const char *what)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) {
        ++g_failures;
    }
}

/**
 * 逐行复刻 `kis_memory_window_test.cpp::testWindow()` 里**建窗口那两行**：
 * 同样的构造函数、同样的参数类型/个数、同样的窗口大小 1024（这个值就是那个测试
 * 选的，不要改——窗口小于一页正是让缺陷暴露的条件）。
 */
std::string makeSwapDir()
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("krita_mw_align_" + std::to_string(::getpid()));
    std::error_code error;
    std::filesystem::remove_all(dir, error);
    std::filesystem::create_directories(dir, error);
    return dir.string();
}

/**
 * 复刻 `testWindow()` 的调用序列（函数、参数、顺序一一对应）：
 *   1. getWriteChunkPtr(chunk1)  写 10 字节
 *   2. getWriteChunkPtr(chunk2)  写 10 字节
 *   3. getReadChunkPtr(chunk2)   读回 10 字节，比对
 *   4. getWriteChunkPtr(chunk1)  再读回 10 字节，比对
 * chunk1 = (0, 10)、chunk2 = (1025, 10) —— 与真实测试逐字相同。
 */
bool runOriginalCallShape(const std::string &swapDir)
{
    KisMemoryWindow memory(PkString(swapDir.c_str()), 1024);

    const unsigned char oddValue = 0xee;
    const unsigned char chunkLength = 10;

    unsigned char oddBuf[chunkLength];
    memset(oddBuf, oddValue, chunkLength);

    KisChunkData chunk1(0, chunkLength);
    KisChunkData chunk2(1025, chunkLength);

    unsigned char *ptr = nullptr;

    ptr = memory.getWriteChunkPtr(chunk1);
    if (!ptr) return false;
    memcpy(ptr, oddBuf, chunkLength);

    ptr = memory.getWriteChunkPtr(chunk2);
    if (!ptr) return false;
    memcpy(ptr, oddBuf, chunkLength);

    ptr = memory.getReadChunkPtr(chunk2);
    if (!ptr) return false;
    if (memcmp(ptr, oddBuf, chunkLength)) return false;

    ptr = memory.getWriteChunkPtr(chunk1);
    if (!ptr) return false;
    if (memcmp(ptr, oddBuf, chunkLength)) return false;

    return true;
}

/**
 * 真实测试只调了一次 `getReadChunkPtr`，覆盖不到「偏移 0 已映射那条分支」
 * （`adjustWindow` 的 early-return：请求落在当前窗口内就直接复用，不再 mmap）。
 * 这里补上，把两半都钉住。**这是本 driver 相对真实测试的额外覆盖，不是复刻。**
 */
bool runExtraCoverage(const std::string &swapDir)
{
    KisMemoryWindow memory(PkString(swapDir.c_str()), 1024);

    // 六个互不重叠（各隔至少 1 KiB）、末尾都不落在页边界上的偏移 —— 除 0 之外
    // 没有一个是页大小的整数倍，正是缺陷成立的条件；0 留作对照组。
    const PkChunkOffsetCase offsets[] = {
        {0, 0x11}, {1025, 0x22}, {4097, 0x33}, {8193, 0x44}, {12289, 0x55}, {16385, 0x66},
    };

    for (const PkChunkOffsetCase &c : offsets) {
        unsigned char buf[16];
        memset(buf, c.value, sizeof(buf));

        unsigned char *ptr = memory.getWriteChunkPtr(KisChunkData(c.offset, sizeof(buf)));
        if (!ptr) return false;
        memcpy(ptr, buf, sizeof(buf));
    }

    for (const PkChunkOffsetCase &c : offsets) {
        unsigned char expected[16];
        memset(expected, c.value, sizeof(expected));

        unsigned char *ptr = memory.getReadChunkPtr(KisChunkData(c.offset, sizeof(expected)));
        if (!ptr) return false;
        if (memcmp(ptr, expected, sizeof(expected))) return false;
    }

    return true;
}

} // namespace

int main()
{
    const std::string swapDir = makeSwapDir();

    /**
     * 声明覆盖不到什么（spec「说不出覆盖不到什么对拍/试接，说明还没想清楚」）：
     *  - 只覆盖 `KisMemoryWindow` 单线程的顺序访问；并发下的窗口换手不在本 driver 范围
     *  - 不覆盖 `KisSwappedDataStore` 那一层（它由 paint_smoke 的端到端证据覆盖）
     *  - 窗口大于默认页大小的边界（请求 chunk 本身 > defaultSize 时 windowSize 被抬到
     *    requestedChunk.size() 那条分支）未覆盖
     *  - 文件增长失败 / mmap 因别的原因失败（磁盘满）不在范围
     */
    std::printf("swapDir=%s\n", swapDir.c_str());
    std::printf("pageSize=%ld\n", ::sysconf(_SC_PAGESIZE));
    check(runOriginalCallShape(swapDir), "复刻 kis_memory_window_test::testWindow 的调用序列");
    check(runExtraCoverage(swapDir), "补覆盖：六个非页对齐偏移的写-读往返");

    std::printf("RESULT=%s failures=%d\n", g_failures == 0 ? "done" : "stuck", g_failures);
    return g_failures == 0 ? 0 : 1;
}
