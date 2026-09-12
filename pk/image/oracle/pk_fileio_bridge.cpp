// pk_fileio_bridge.cpp —— `image_difftest.cpp` 调 `PkImage` **文件 I/O** 的桥。
//
// ── 为什么需要这座桥（而不是像其它 API 那样把 .cpp 内联进对拍 TU）────────
// `image_difftest.cpp` 把 pk/image 的源码放进 `namespace pkoracle { ... }` 展开，
// 于是它调用的是 `pkoracle::PkImage`。这对**内存操作**成立（`PkImage.cpp` 里
// 有定义），但 `PkImage(PkString)`/`load`/`save` 的**定义在
// `pk/image/PkImageFileIo.cpp`、编进 `pkimageio`**（`libpkimage.a` 里这三个是
// 未定义符号，见 `pk/image/README.md`）。把它们内联进对拍 TU 需要连带把
// `PkImageFileDecoder.cpp` / `PkPngWriter.cpp` / `PkPngReader.cpp` /
// `codecs/*.cpp` 一起卷进来并链 5 个编解码库——那是把 `pkimageio` 整个重编一遍。
//
// 所以这里反过来做：本文件在**全局作用域** include `PkImage.h`（不套 namespace），
// 导出 `extern "C"` 包装，链**真编出来的** `libpkimage.a` + `libpkimageio.dylib`。
// 对拍对象因此是「对拍 TU 外的真交付物」而不是「TU 里展开的一份拷贝」。
//
// ⚠ 对本组而言这**更强**、不是更弱：单测 `test_pkimage` 验的也正是这三个符号的
// 链接期落地（`pkimageio` 补齐 `pkimage` 的未定义符号），桥走的是同一条路径。
//
// 内存所有权：`pkbridge_load`/`pkbridge_invert` 返回 `malloc` 的缓冲区，调用方
// 必须 `pkbridge_free`。用 malloc/free 而不是 new/delete，是为了让桥的 ABI 只依赖
// C 运行时——对拍程序与桥是**两个 TU、同一进程**，混用分配器在这里本来没事，
// 但边界干净点没坏处。

#include "PkImage.h"

#include <cstdlib>
#include <cstring>

extern "C" {

// 读一张图：`PkImage(path)`（即按路径构造 → 内部走解码），再折成 ARGB32
// （与 `QImage::convertToFormat(QImage::Format_ARGB32)` 对等的一侧），
// 返回 width*height 个 **ARGB32 打包字**。
// 失败（打不开 / null）返回 nullptr，*outW/*outH 置 0、*outFormat 置 -1。
unsigned int *pkbridge_load(const char *path, int *outW, int *outH, int *outFormat)
{
    if (outW) *outW = 0;
    if (outH) *outH = 0;
    if (outFormat) *outFormat = -1;

    // ⚠ 不能写 `const PkImage loaded(PkString(path));`——那是 vexing parse，
    // 编译器会当成函数声明（参数名叫 path、类型是 PkString）。用拷贝初始化。
    const PkImage loaded = PkImage(PkString(path));
    if (loaded.isNull()) {
        return nullptr;
    }
    if (outFormat) *outFormat = static_cast<int>(loaded.format());

    const int w = loaded.width();
    const int h = loaded.height();
    const PkImage argb = (loaded.format() == PkImage::Format_ARGB32)
        ? loaded
        : loaded.convertToFormat(PkImage::Format_ARGB32);

    unsigned int *buffer = static_cast<unsigned int *>(
        std::malloc(sizeof(unsigned int) * static_cast<size_t>(w) * static_cast<size_t>(h)));
    if (!buffer) {
        return nullptr;
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            buffer[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
                argb.pixel(x, y);
        }
    }
    if (outW) *outW = w;
    if (outH) *outH = h;
    return buffer;
}

// 读一张图的**原始格式码**（`PkImage::format()` 的整数值），不读像素。
// 打不开返回 -1。用于把「解码出来的格式」也纳入对拍。
int pkbridge_load_format(const char *path)
{
    const PkImage loaded = PkImage(PkString(path)); // vexing parse 同上
    if (loaded.isNull()) {
        return -1;
    }
    return static_cast<int>(loaded.format());
}

// 写一张图：用给定 ARGB32 像素造 `PkImage(w, h, Format_ARGB32)` 再 `save`。
// `format` 可为 nullptr（走路径后缀），`quality` 传 -1 表示默认。
// 返回 1 成功 / 0 失败（`save` 的返回值本身）。
int pkbridge_save(const char *path, const unsigned int *pixels, int w, int h,
                  const char *format, int quality)
{
    if (!pixels || w <= 0 || h <= 0) {
        return 0;
    }
    PkImage image(w, h, PkImage::Format_ARGB32);
    if (image.isNull()) {
        return 0;
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            image.setPixel(x, y,
                           pixels[static_cast<size_t>(y) * static_cast<size_t>(w)
                                  + static_cast<size_t>(x)]);
        }
    }
    return image.save(PkString(path), format, quality) ? 1 : 0;
}

// 原地取反：造 `PkImage(w, h, formatCode)` 图 → `invertPixels(mode)` → 导出
// ARGB32 像素。`formatCode` 取 `PkImage::Format` 的整数值（与 `QImage::Format`
// 同序，本 TU 的 static_assert 钉过）——invertPixels 对预乘/非预乘走**不同**
// 路径，格式必须能参数化，只测 ARGB32 会漏掉一半行为。
// mode: 0 = InvertRgb，1 = InvertRgba（与 `QImage::InvertRgb`/`InvertRgba` 同序）。
// 返回 malloc 的缓冲区（调用方 pkbridge_free），失败返回 nullptr。
unsigned int *pkbridge_invert(const unsigned int *pixels, int w, int h,
                              int formatCode, int mode)
{
    if (!pixels || w <= 0 || h <= 0) {
        return nullptr;
    }
    PkImage image(w, h, static_cast<PkImage::Format>(formatCode));
    if (image.isNull()) {
        return nullptr;
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            image.setPixel(x, y,
                           pixels[static_cast<size_t>(y) * static_cast<size_t>(w)
                                  + static_cast<size_t>(x)]);
        }
    }
    image.invertPixels(mode == 1 ? PkImage::InvertRgba : PkImage::InvertRgb);

    unsigned int *buffer = static_cast<unsigned int *>(
        std::malloc(sizeof(unsigned int) * static_cast<size_t>(w) * static_cast<size_t>(h)));
    if (!buffer) {
        return nullptr;
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            buffer[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] =
                image.pixel(x, y);
        }
    }
    return buffer;
}

void pkbridge_free(void *buffer)
{
    std::free(buffer);
}

} // extern "C"
