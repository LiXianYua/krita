#pragma once

#include <cstdint>
#include <vector>

#include "../geometry/PkGlobal.h"   // qreal

// PkImageData —— PkImage 的内部表示，由 PkArrayData<PkImageData> 持有并做 COW。
// 不是公开 API 的一部分，只在 PkImage.h/.cpp 内使用。
struct PkImageData
{
    int width = 0;
    int height = 0;
    int bytesPerLine = 0;
    int format = 0;                       // 存 PkImage::Format_Invalid 的整数值（0）
    qreal devicePixelRatio = 1.0;
    std::vector<uint8_t> pixels;          // 裸像素字节，按 bytesPerLine 步进
    std::vector<uint32_t> colorTable;     // 仅 Indexed8/Mono/MonoLSB 使用，ARGB32 打包
};
