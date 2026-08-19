#pragma once

#include <cstdint>
#include <vector>

#include "PkImageData.h"
#include "../container/PkArrayData.h"
#include "../geometry/PkSize.h"
#include "../geometry/PkRect.h"
#include "../geometry/PkGlobal.h"   // Qt::GlobalColor（fill(Qt::GlobalColor) 用）

class PkImage
{
public:
    enum Format {
        Format_Invalid,
        Format_Mono,
        Format_MonoLSB,
        Format_Indexed8,
        Format_RGB32,
        Format_ARGB32,
        Format_ARGB32_Premultiplied,
        Format_RGB16,
        Format_ARGB8565_Premultiplied,
        Format_RGB666,
        Format_ARGB6666_Premultiplied,
        Format_RGB555,
        Format_ARGB8555_Premultiplied,
        Format_RGB888,
        Format_RGB444,
        Format_ARGB4444_Premultiplied,
        Format_RGBX8888,
        Format_RGBA8888,
        Format_RGBA8888_Premultiplied,
        Format_BGR30,
        Format_A2BGR30_Premultiplied,
        Format_RGB30,
        Format_A2RGB30_Premultiplied,
        Format_Alpha8,
        Format_Grayscale8,
        Format_RGBX64,
        Format_RGBA64,
        Format_RGBA64_Premultiplied,
        Format_Grayscale16,
        Format_BGR888
    };

    PkImage();
    PkImage(int width, int height, Format format);
    PkImage(const PkSize &size, Format format);

    PkImage(const PkImage &other) = default;
    PkImage(PkImage &&other) noexcept = default;
    PkImage &operator=(const PkImage &other) = default;
    PkImage &operator=(PkImage &&other) noexcept = default;
    ~PkImage() = default;

    Format format() const;

    int width() const;
    int height() const;
    PkSize size() const;
    PkRect rect() const;

    bool isNull() const;
    int depth() const;
    int bytesPerLine() const;
    long long sizeInBytes() const;
    int colorCount() const;

    // ---- 像素访问与写入（Task 2）----
    //
    // scanLine()/bits() 非 const 重载经 PkArrayData::PkMut()，触发 detach；
    // constScanLine()/constBits() 经 PkConst()，绝不 detach（探针第 4 组硬约束）。
    // 行号越界（y<0 或 y>=height()）返回 nullptr——目前无真实调用点覆盖越界行为，
    // 选 nullptr 而不是做越界指针算术：构造越界指针本身就是 UB，即使不解引用；
    // 返回哨兵指针是更安全的确定性选择，调用方要检查空指针本就是常见约定。
    uint8_t *scanLine(int y);
    const uint8_t *constScanLine(int y) const;
    uint8_t *bits();
    const uint8_t *constBits() const;

    // pixel()/setPixel()：对 Indexed8/Mono/MonoLSB，value 是颜色表**索引**
    // （探针第 8 组实测：setPixel(0,0,1) 后 pixelIndex()==1、pixel() 经颜色表
    // 查出 0xffffffff）；对直接色格式，value 是打包好的 ARGB32（0xAARRGGBB）。
    //
    // pixelColor()/setPixelColor() 本该是 QColor 等价物，但仓库目前没有
    // PkColor/等价颜色值类型（选型文档范围内没有排期对应任务）——这里直接复用
    // pixel()/setPixel() 的 uint32_t ARGB32 打包值作为参数/返回值类型，不为这一
    // 个方法新造类型；等 PkColor/等价物落地后再改接口签名。setPixelColor() 在
    // 索引格式（Mono/MonoLSB/Indexed8）上是 no-op，对齐真 Qt 的记录行为
    // （真 Qt 对索引格式调用 setPixelColor() 会 warning 后什么都不做）。
    //
    // 越界坐标（x<0/y<0/x>=width()/y>=height()）：pixel()/pixelColor() 返回 0
    // （透明黑），pixelIndex() 返回 0，setPixel()/setPixelColor() 是 no-op——
    // 全部是确定性行为，不是 UB，不崩溃。
    uint32_t pixel(int x, int y) const;
    void setPixel(int x, int y, uint32_t indexOrRgb);
    uint32_t pixelColor(int x, int y) const;
    void setPixelColor(int x, int y, uint32_t argb);
    int pixelIndex(int x, int y) const;

    // fill(uint)：value 按 pixel()/setPixel() 同一套打包/索引约定逐像素写入
    // （对 ARGB32/ARGB32_Premultiplied 是裸值透传，探针第 10 组实测）。
    // fill(Qt::GlobalColor)：只保证 white/black/red/gray/transparent 5 个真实
    // 调用点用到的值精确（PkGlobal.h 顶部注释里的探针实测 RGBA），其余 15 个
    // GlobalColor 值传入时不保证精确颜色（判据①，允许 no-op）。
    void fill(uint32_t value);
    void fill(Qt::GlobalColor color);

    // colorTable()/setColorTable()：仅 Indexed8/Mono/MonoLSB 有意义；非索引
    // 格式上 colorTable() 恒返回空表（探针第 9 组实测），setColor()/setColorTable()
    // 在非索引格式上不保证有意义的行为，但保证不崩溃（对拍不覆盖这个组合，
    // plan「甲类对拍」小节，留给 Task 4）。setColor(i, argb) 越界时自动扩容
    // colorTable（探针第 8 组旁注）；setColorCount(n) 即 colorTable.resize(n)。
    std::vector<uint32_t> colorTable() const;
    void setColorTable(const std::vector<uint32_t> &colors);
    void setColorCount(int colorCount);
    uint32_t color(int index) const;
    void setColor(int index, uint32_t argbColor);

    // 逐像素判断 R==G==B（alpha 无关）。Grayscale8/Alpha8/Mono/MonoLSB 本身就是
    // 灰度语义，直接返回 true（探针第 13 组：按定义实现，未专门探测这些格式）。
    bool allGray() const;

    // ---- 只给单测用，不进 compat 垫片（先例：pk/container/PkArrayContainer.h）----
    // 真 Qt 的 isDetached()/isSharedWith() 在 Krita 调用点实测 0 处，这两个
    // Pk 前缀观测器只服务「写方法漏用 PkMut()」这类 detach 时机回归单测。
    long PkUseCount() const noexcept;
    bool PkIsSharedWith(const PkImage &other) const noexcept;

private:
    PkArrayData<PkImageData> m_d;
};
