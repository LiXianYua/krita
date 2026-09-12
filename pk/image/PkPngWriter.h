#pragma once

#include "PkImage.h"
#include "PkImageIoExport.h"

#include <cstdint>
#include <vector>

// R-75：libpng 写侧。仓库此前只有读侧（PkPngReader），没有任何 image encoder；
// `PkImage::save` 靠这个类真的把像素写出去（不是空实现/恒 false 的假绿）。
//
// 输出固定为 8bit PNG（直通 alpha，即非预乘）：源图是预乘格式时逐像素先反预乘。
// 颜色类型按内容选——所有像素都完全不透明时写 RGB（color type 2），否则写
// RGBA（color type 6）。这样 `load -> save -> load` 逐像素相等。
//
// 支持的源格式（与 `PkImage.cpp` 的 rawPixelArgb 可读集合一致）：
// ARGB32 / RGB32 / ARGB32_Premultiplied / RGBA8888 / Grayscale8 / Indexed8 /
// Mono / MonoLSB。其余格式（含 16bit/打包位/PkImage 尚未做像素级读的格式）
// 返回空 vector，`PkImage::save` 据此返回 false——不静默写错数据。
class PKIMAGEIO_EXPORT PkPngWriter
{
public:
    // quality 对 PNG（无损）没有语义，真 Qt 的 QImage::save 也照收；为签名对称
    // 保留实参，写入时忽略。
    static std::vector<uint8_t> write(const PkImage &image, int quality = -1);
};
