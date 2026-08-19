#pragma once

#include <cstdint>
#include <vector>

#include "PkImageData.h"
#include "../container/PkArrayData.h"
#include "../geometry/PkSize.h"
#include "../geometry/PkRect.h"
#include "../geometry/PkGlobal.h"   // Qt::GlobalColor（fill(Qt::GlobalColor) 用）
#include "../geometry/PkTransform.h"   // Task 3：scaled()/transformed() 用

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
    const uint8_t *scanLine(int y) const; // 修复轮 1：转发 constScanLine，绝不 detach
    const uint8_t *constScanLine(int y) const;
    uint8_t *bits();
    const uint8_t *bits() const; // 修复轮 1：转发 constBits，绝不 detach
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

    // 逐像素判断 R==G==B（alpha 无关）。语义对齐真 Qt QImage::allGray()：
    // Grayscale8/16 恒 true；Alpha8 返回 false（纯 alpha 不是灰度）；Mono/
    // MonoLSB/Indexed8 逐项检查颜色表是否全灰（任一非灰即 false）；32bpp 直接
    // 色与其余格式逐像素判 R==G==B。
    bool allGray() const;

    // ---- Task 3：格式转换、派生操作 ----
    //
    // copy()：无条件深拷贝（探针第 7 组，same-ptr=0，即使原本没有共享者也强制
    // 产生新分配）。convertToFormat()：同格式共享而非拷贝（探针第 6 组）；不同
    // 格式时逐像素转换，复用 rawPixelArgb/writeRawPixelArgb。convertTo()：原地
    // 版本，语义 `*this = convertToFormat(newFormat)`；真 Qt 还有一个
    // Qt::ImageConversionFlags 参数，但用量表没有任何真实调用点带这个参数，
    // 故只提供单参数重载（判据①，不多加）。
    PkImage copy() const;
    PkImage convertToFormat(Format newFormat) const;

    // Fix round 1（评审后追加，libs/brush/kis_svg_brush.cpp:53 真实调用点需要）：
    // 调用方指定调色板的重载——QImage::convertToFormat(Format, const
    // QVector<QRgb>&, ...) 的等价物，用 std::vector<uint32_t> 代替 QVector<QRgb>
    // （本仓零 Qt，打包约定与 pixel()/color() 一致）。对源图每像素求 ARGB32 值
    // （复用 rawPixelArgb），在 colorTable 里找 R/G/B 欧氏距离最近的一项，把其
    // 索引写入目标像素（同 writeRawPixelIndex 的既有索引写入约定）；忽略 alpha
    // 分量参与距离计算，理由见 PkImage.cpp 实现处注释。目标图像的 colorTable
    // 字段是传入 colorTable 的原样拷贝。空 colorTable 时全部像素索引保持零
    // 初始化状态，不崩溃。
    PkImage convertToFormat(Format targetFormat, const std::vector<uint32_t> &colorTable) const;

    void convertTo(Format newFormat);

    // scaled()/transformed()：Fast（最近邻）模式精确复刻真 Qt 探针实测的映射
    // 公式；Smooth（双线性）模式是已声明偏离（岔路 B），不追求与 Qt 位对齐，只
    // 保证良定义。devicePixelRatio 在两者之后原样透传（探针结论 4）。
    PkImage scaled(const PkSize &size,
                   Qt::AspectRatioMode aspectMode = Qt::IgnoreAspectRatio,
                   Qt::TransformationMode mode = Qt::FastTransformation) const;
    PkImage transformed(const PkTransform &matrix,
                         Qt::TransformationMode mode = Qt::FastTransformation) const;

    qreal devicePixelRatio() const;
    void setDevicePixelRatio(qreal scaleFactor);

    // operator==：深度像素内容比较（探针第 5 组）。共享指针相等是短路优化，不
    // 共享时逐字节比较 format/width/height/bytesPerLine/pixels/colorTable；不
    // 比较 devicePixelRatio（对齐真 Qt QImage::operator== 的语义）。
    bool operator==(const PkImage &other) const;
    bool operator!=(const PkImage &other) const;

    // ---- 只给单测用，不进 compat 垫片（先例：pk/container/PkArrayContainer.h）----
    // 真 Qt 的 isDetached()/isSharedWith() 在 Krita 调用点实测 0 处，这两个
    // Pk 前缀观测器只服务「写方法漏用 PkMut()」这类 detach 时机回归单测。
    long PkUseCount() const noexcept;
    bool PkIsSharedWith(const PkImage &other) const noexcept;

private:
    PkArrayData<PkImageData> m_d;
};
