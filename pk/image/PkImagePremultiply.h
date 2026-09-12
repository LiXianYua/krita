#pragma once

// R-75：预乘 / 反预乘辅助，逐字照抄真 Qt 5.15.7 `QtGui/qrgb.h` 的
// `qUnpremultiply`（:96-108）与 `qPremultiply`（:81-92）。
//
// 为什么放独立头：两处需要同一份公式——
//   · `PkImage.cpp` 的 `invertPixels`：预乘格式的取反链
//     `qUnpremultiply -> 按位取反 -> qPremultiply`（Qt 走的是
//     convertToFormat(非预乘) -> 取反 -> convertToFormat(预乘)，内核即这两条）；
//   · `PkPngWriter.cpp`：PNG 存的是直通（非预乘）alpha，预乘源图必须先反预乘
//     再把像素交给编码器。
// 放头里共享，避免两份逐字照抄各自漂移。
//
// 本头是 `pk/image` 的**内部实现细节**：不进公共 API、不加导出宏、无独立编译
// 单元（所以不需要列进任何 CMake 源列表）。
//
// 实测依据（探针 `probe_unpre.cpp`，全 256×256 组合 0 diff）：
// `qt_inv_premul_factor[alpha]` 等于 `0x00ff00ff / alpha` 的整数除。
// 加 `0x8000` 是 Qt 的偶数舍入，它保证 `qPremultiply(qUnpremultiply(p)) == p`。

#include <cstdint>

namespace pkimage_detail
{

inline uint8_t pmAlpha(uint32_t c) { return static_cast<uint8_t>(c >> 24); }
inline uint8_t pmRed(uint32_t c) { return static_cast<uint8_t>(c >> 16); }
inline uint8_t pmGreen(uint32_t c) { return static_cast<uint8_t>(c >> 8); }
inline uint8_t pmBlue(uint32_t c) { return static_cast<uint8_t>(c); }
inline uint32_t pmPack(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
    return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

inline uint32_t unpremultiply(uint32_t p)
{
    const uint32_t a = pmAlpha(p);
    // Alpha 255 与 0 是两个最常见值，Qt 也短路（qrgb.h:99-103）。
    if (a == 255) {
        return p;
    }
    if (a == 0) {
        return 0;
    }
    const uint32_t invAlpha = 0x00ff00ffu / a;
    const uint32_t r = ((uint32_t(pmRed(p)) * invAlpha + 0x8000u) >> 16) & 0xffu;
    const uint32_t g = ((uint32_t(pmGreen(p)) * invAlpha + 0x8000u) >> 16) & 0xffu;
    const uint32_t b = ((uint32_t(pmBlue(p)) * invAlpha + 0x8000u) >> 16) & 0xffu;
    return pmPack(static_cast<uint8_t>(a), static_cast<uint8_t>(r),
                  static_cast<uint8_t>(g), static_cast<uint8_t>(b));
}

inline uint32_t premultiply(uint32_t x)
{
    const uint32_t a = x >> 24;
    uint32_t t = (x & 0xff00ffu) * a;
    t = (t + ((t >> 8) & 0xff00ffu) + 0x800080u) >> 8;
    t &= 0xff00ffu;

    x = ((x >> 8) & 0xffu) * a;
    x = (x + ((x >> 8) & 0xffu) + 0x80u);
    x &= 0xff00u;
    return x | t | (a << 24);
}

} // namespace pkimage_detail
