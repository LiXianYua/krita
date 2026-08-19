#include "PkImage.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace {

int depthTable(PkImage::Format format)
{
    switch (format) {
    case PkImage::Format_Invalid:                  return 0;
    case PkImage::Format_Mono:                      return 1;
    case PkImage::Format_MonoLSB:                   return 1;
    case PkImage::Format_Indexed8:                  return 8;
    case PkImage::Format_RGB32:                      return 32;
    case PkImage::Format_ARGB32:                     return 32;
    case PkImage::Format_ARGB32_Premultiplied:       return 32;
    case PkImage::Format_RGB16:                      return 16;
    case PkImage::Format_ARGB8565_Premultiplied:     return 24;
    case PkImage::Format_RGB666:                     return 24;
    case PkImage::Format_ARGB6666_Premultiplied:     return 24;
    case PkImage::Format_RGB555:                     return 16;
    case PkImage::Format_ARGB8555_Premultiplied:     return 24;
    case PkImage::Format_RGB888:                     return 24;
    case PkImage::Format_RGB444:                     return 16;
    case PkImage::Format_ARGB4444_Premultiplied:     return 16;
    case PkImage::Format_RGBX8888:                   return 32;
    case PkImage::Format_RGBA8888:                   return 32;
    case PkImage::Format_RGBA8888_Premultiplied:     return 32;
    case PkImage::Format_BGR30:                       return 32;
    case PkImage::Format_A2BGR30_Premultiplied:       return 32;
    case PkImage::Format_RGB30:                       return 32;
    case PkImage::Format_A2RGB30_Premultiplied:       return 32;
    case PkImage::Format_Alpha8:                       return 8;
    case PkImage::Format_Grayscale8:                    return 8;
    case PkImage::Format_RGBX64:                       return 64;
    case PkImage::Format_RGBA64:                       return 64;
    case PkImage::Format_RGBA64_Premultiplied:          return 64;
    case PkImage::Format_Grayscale16:                    return 16;
    case PkImage::Format_BGR888:                        return 24;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Task 2：像素级读写辅助。
//
// ARGB32 打包约定：uint32_t 从高到低字节 = A R G B（0xAARRGGBB），与真 Qt 的
// QRgb 完全一致；pixel()/pixelColor()/setPixel()/setPixelColor()/fill(uint)
// 全部统一用这一种打包格式（仓库目前没有 PkColor/等价颜色值类型，用裸
// uint32_t 代替 QColor 是简化决策，见 PkImage.h 头注释）。
// ---------------------------------------------------------------------------

inline uint8_t argbAlpha(uint32_t c) { return static_cast<uint8_t>(c >> 24); }
inline uint8_t argbRed(uint32_t c) { return static_cast<uint8_t>(c >> 16); }
inline uint8_t argbGreen(uint32_t c) { return static_cast<uint8_t>(c >> 8); }
inline uint8_t argbBlue(uint32_t c) { return static_cast<uint8_t>(c); }
inline uint32_t packArgb(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
{
    return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}

// qGray 公式，逐字照抄真 Qt qcolor.h 的 qGray(int,int,int)：(r*11+g*16+b*5)/32。
// Grayscale8 的 setPixel/setPixelColor 用它把任意 ARGB32 值折算成灰度字节。
inline uint8_t qGrayOf(uint8_t r, uint8_t g, uint8_t b)
{
    return static_cast<uint8_t>((int(r) * 11 + int(g) * 16 + int(b) * 5) / 32);
}

bool coordInBounds(const PkImageData &d, int x, int y)
{
    return x >= 0 && y >= 0 && x < d.width && y < d.height;
}

const uint8_t *rowPtr(const PkImageData &d, int y)
{
    return d.pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(d.bytesPerLine);
}

uint8_t *rowPtrMut(PkImageData &d, int y)
{
    return d.pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(d.bytesPerLine);
}

// Mono/MonoLSB/Indexed8 的颜色表查表，越界给确定的兜底值（不崩溃）：
// Mono/MonoLSB 未显式 setColor 时兜底 index0=不透明黑、index1=不透明白
// （对齐 Qt 单色调色板的常见默认语义）；Indexed8 没有"默认两色"这种语义，
// 越界纯粹是调用方还没 setColorCount 就来读，兜底透明黑（0）,不编造颜色。
uint32_t lookupColorTable(const PkImageData &d, PkImage::Format format, int index)
{
    const std::vector<uint32_t> &table = d.colorTable;
    if (index >= 0 && static_cast<size_t>(index) < table.size()) {
        return table[static_cast<size_t>(index)];
    }
    if (format == PkImage::Format_Mono || format == PkImage::Format_MonoLSB) {
        return index == 1 ? 0xFFFFFFFFu : 0xFF000000u;
    }
    return 0u;
}

// 读取 Indexed8/Mono/MonoLSB 的原始索引，与颜色表无关。非索引格式返回 0
// （真 Qt 对非索引格式调用 pixelIndex() 会 warning 后返回 0，同口径）。
int rawPixelIndex(const PkImageData &d, PkImage::Format format, int x, int y)
{
    switch (format) {
    case PkImage::Format_Indexed8: {
        const uint8_t *row = rowPtr(d, y);
        return static_cast<int>(row[x]);
    }
    case PkImage::Format_Mono: {
        const uint8_t *row = rowPtr(d, y);
        uint8_t byte = row[x / 8];
        return (byte >> (7 - (x % 8))) & 1;
    }
    case PkImage::Format_MonoLSB: {
        const uint8_t *row = rowPtr(d, y);
        uint8_t byte = row[x / 8];
        return (byte >> (x % 8)) & 1;
    }
    default:
        return 0;
    }
}

void writeRawPixelIndex(PkImageData &d, PkImage::Format format, int x, int y, uint32_t value)
{
    switch (format) {
    case PkImage::Format_Indexed8: {
        uint8_t *row = rowPtrMut(d, y);
        row[x] = static_cast<uint8_t>(value & 0xFFu);
        return;
    }
    case PkImage::Format_Mono: {
        uint8_t *row = rowPtrMut(d, y);
        uint8_t mask = static_cast<uint8_t>(1u << (7 - (x % 8)));
        if (value & 1u) {
            row[x / 8] |= mask;
        } else {
            row[x / 8] &= static_cast<uint8_t>(~mask);
        }
        return;
    }
    case PkImage::Format_MonoLSB: {
        uint8_t *row = rowPtrMut(d, y);
        uint8_t mask = static_cast<uint8_t>(1u << (x % 8));
        if (value & 1u) {
            row[x / 8] |= mask;
        } else {
            row[x / 8] &= static_cast<uint8_t>(~mask);
        }
        return;
    }
    default:
        return;
    }
}

// 高频直接色格式（brief 判据①列出的 9 个之内的非索引部分）：原始存储 <->
// ARGB32 打包值互转。default 分支覆盖低频 packed-bit 格式（RGB666/RGB555/
// RGB444/ARGB6666_Premultiplied 等）以及不在 9 个高频格式清单内的其它格式
// （例如 RGBA8888_Premultiplied/RGBA64_Premultiplied/RGB888 等）——这些格式
// Task 1 已经保证 depth()/bytesPerLine() 正确，像素级读写不在本 Task 范围
// （判据①，一项不多），给占位值 0 并在 debug 构建下 assert 提醒，不崩溃。
uint32_t rawPixelArgb(const PkImageData &d, PkImage::Format format, int x, int y)
{
    const uint8_t *row = rowPtr(d, y);
    switch (format) {
    case PkImage::Format_RGB32: {
        uint32_t v;
        std::memcpy(&v, row + x * 4, 4);
        return v | 0xFF000000u; // RGB32 的 alpha 字节不可信，读出时强制不透明
    }
    case PkImage::Format_ARGB32:
    case PkImage::Format_ARGB32_Premultiplied: {
        uint32_t v;
        std::memcpy(&v, row + x * 4, 4);
        return v; // 原生字长本身就是 0xAARRGGBB 打包，裸传即可
    }
    case PkImage::Format_RGBA8888: {
        const uint8_t *p = row + x * 4; // 内存字节序固定 R,G,B,A，与主机字节序无关
        return packArgb(p[3], p[0], p[1], p[2]);
    }
    case PkImage::Format_Indexed8:
    case PkImage::Format_Mono:
    case PkImage::Format_MonoLSB: {
        int idx = rawPixelIndex(d, format, x, y);
        return lookupColorTable(d, format, idx);
    }
    case PkImage::Format_Grayscale8: {
        uint8_t g = row[x];
        return packArgb(0xFF, g, g, g);
    }
    case PkImage::Format_RGBA64: {
        // 64bit：内存排布 R,G,B,A 各 16bit（主机字节序），对应真 Qt QRgba64 的
        // 内部字段顺序（qrgba64.h：red()=bit0-15 green()=16-31 blue()=32-47
        // alpha()=48-63）。降采样到 8bit 用高字节（>>8）。
        uint64_t v;
        std::memcpy(&v, row + x * 8, 8);
        uint16_t r16 = static_cast<uint16_t>(v);
        uint16_t g16 = static_cast<uint16_t>(v >> 16);
        uint16_t b16 = static_cast<uint16_t>(v >> 32);
        uint16_t a16 = static_cast<uint16_t>(v >> 48);
        return packArgb(static_cast<uint8_t>(a16 >> 8), static_cast<uint8_t>(r16 >> 8),
                         static_cast<uint8_t>(g16 >> 8), static_cast<uint8_t>(b16 >> 8));
    }
    default:
        assert(false && "PkImage: pixel-level read not implemented for this low-frequency format (placeholder 0)");
        return 0;
    }
}

void writeRawPixelArgb(PkImageData &d, PkImage::Format format, int x, int y, uint32_t value)
{
    uint8_t *row = rowPtrMut(d, y);
    switch (format) {
    case PkImage::Format_RGB32: {
        uint32_t v = value | 0xFF000000u; // 强制不透明
        std::memcpy(row + x * 4, &v, 4);
        return;
    }
    case PkImage::Format_ARGB32:
    case PkImage::Format_ARGB32_Premultiplied: {
        std::memcpy(row + x * 4, &value, 4); // 裸值透传，不做任何解释（探针第10组）
        return;
    }
    case PkImage::Format_RGBA8888: {
        uint8_t *p = row + x * 4;
        p[0] = argbRed(value);
        p[1] = argbGreen(value);
        p[2] = argbBlue(value);
        p[3] = argbAlpha(value);
        return;
    }
    case PkImage::Format_Indexed8:
    case PkImage::Format_Mono:
    case PkImage::Format_MonoLSB: {
        writeRawPixelIndex(d, format, x, y, value); // value 是颜色表索引，探针第8组
        return;
    }
    case PkImage::Format_Grayscale8: {
        row[x] = qGrayOf(argbRed(value), argbGreen(value), argbBlue(value));
        return;
    }
    case PkImage::Format_RGBA64: {
        // 8bit -> 16bit 用 c*257（即 (c<<8)|c）展宽，是 Qt 常见的整数精确展宽公式。
        auto expand = [](uint8_t c) -> uint16_t { return static_cast<uint16_t>((uint16_t(c) << 8) | c); };
        uint16_t r16 = expand(argbRed(value));
        uint16_t g16 = expand(argbGreen(value));
        uint16_t b16 = expand(argbBlue(value));
        uint16_t a16 = expand(argbAlpha(value));
        uint64_t v = uint64_t(r16) | (uint64_t(g16) << 16) | (uint64_t(b16) << 32) | (uint64_t(a16) << 48);
        std::memcpy(row + x * 8, &v, 8);
        return;
    }
    default:
        assert(false && "PkImage: pixel-level write not implemented for this low-frequency format (no-op)");
        return;
    }
}

// Qt::GlobalColor -> ARGB32。只保证 5 个真实调用点用到的值精确（+黑顺手做），
// 其余 15 个值判据①范围外，兜底返回 0（透明黑），不保证精确颜色。
uint32_t globalColorToArgb(Qt::GlobalColor color)
{
    switch (color) {
    case Qt::white:       return packArgb(255, 255, 255, 255);
    case Qt::black:       return packArgb(255, 0, 0, 0);
    case Qt::red:         return packArgb(255, 255, 0, 0);
    case Qt::gray:        return packArgb(255, 160, 160, 164);
    case Qt::transparent: return packArgb(0, 0, 0, 0);
    default:
        return 0u;
    }
}

// ---------------------------------------------------------------------------
// Fix round 1（评审后追加）：convertToFormat(Format, colorTable) 的最近色匹配。
//
// 真实调用点 libs/brush/kis_svg_brush.cpp:53 用的是 QImage 的第二重载
// （调用方指定调色板），当前只实现了单参数版本对索引目标格式是 no-op——这里
// 补上真正的最近色匹配算法。
// ---------------------------------------------------------------------------

// 在给定调色板里找与 argb 的 R/G/B 欧氏距离最近的一项，返回其索引。
//
// 忽略 alpha 分量参与距离计算：调色板项本身代表的是可见颜色，真实调用点
// kis_svg_brush.cpp 构造的调色板是恒不透明的灰度表（qRgb(i,i,i) 打包时 alpha
// 恒为 0xff），把源像素的 alpha 计入距离只会给所有候选项加上同一个常数偏移，
// 不改变排序结果，对这个真实场景是等价的；忽略 alpha 也是更通用、更符合直觉
// 的「颜色匹配」定义——调色板匹配关心的是可见颜色本身，透明度是另一个维度。
// 平局（多个项距离相等）保留线性扫描先出现的索引，确定性、可复现，不依赖
// std::min_element 之类的实现定义的平局规则。
int nearestColorIndex(uint32_t argb, const std::vector<uint32_t> &colorTable)
{
    int r = argbRed(argb);
    int g = argbGreen(argb);
    int b = argbBlue(argb);

    int bestIndex = 0;
    long bestDist = -1;
    for (size_t i = 0; i < colorTable.size(); ++i) {
        uint32_t c = colorTable[i];
        int dr = r - argbRed(c);
        int dg = g - argbGreen(c);
        int db = b - argbBlue(c);
        long dist = static_cast<long>(dr) * dr + static_cast<long>(dg) * dg + static_cast<long>(db) * db;
        if (bestDist < 0 || dist < bestDist) {
            bestDist = dist;
            bestIndex = static_cast<int>(i);
        }
    }
    return bestIndex;
}

// ---------------------------------------------------------------------------
// Task 3：scaled()/transformed() 的 Smooth（双线性）模式辅助函数。
//
// 这是已声明偏离 Qt 的自定义实现（brief「岔路 B」），不追求与 Qt 位对齐，只
// 要求良定义。坐标系与 Fast 模式共用同一套（结论 3）：整数 i 表示第 i 个像素
// 占据连续区间 [i, i+1)，像素中心在 i+0.5。
// ---------------------------------------------------------------------------

// 越界坐标夹到最近边缘像素（clamp-to-edge），而不是补 0——补 0 会在缩放/旋转
// 的边缘产生透明发黑的重影，clamp 是光栅库常见的双线性缺省寻址方式，视觉上
// 更合理。这条本来就不要求跟 Qt 比对，选哪种都成立，写清楚理由即可。
inline int clampCoord(int v, int maxExclusive)
{
    if (v < 0) return 0;
    if (v >= maxExclusive) return maxExclusive - 1;
    return v;
}

// 四个分量（A/R/G/B）分别做双线性插值，四舍五入回 uint8。
uint32_t bilerpArgb(uint32_t c00, uint32_t c10, uint32_t c01, uint32_t c11, double fx, double fy)
{
    auto blendChannel = [fx, fy](uint8_t v00, uint8_t v10, uint8_t v01, uint8_t v11) -> uint8_t {
        double top = v00 + (double(v10) - v00) * fx;
        double bottom = v01 + (double(v11) - v01) * fx;
        double val = top + (bottom - top) * fy;
        return static_cast<uint8_t>(val + 0.5);
    };
    uint8_t a = blendChannel(argbAlpha(c00), argbAlpha(c10), argbAlpha(c01), argbAlpha(c11));
    uint8_t r = blendChannel(argbRed(c00), argbRed(c10), argbRed(c01), argbRed(c11));
    uint8_t g = blendChannel(argbGreen(c00), argbGreen(c10), argbGreen(c01), argbGreen(c11));
    uint8_t b = blendChannel(argbBlue(c00), argbBlue(c10), argbBlue(c01), argbBlue(c11));
    return packArgb(a, r, g, b);
}

} // namespace

PkImage::PkImage() = default;

PkImage::PkImage(int width, int height, Format format)
{
    if (width <= 0 || height <= 0 || format == Format_Invalid) {
        // 保持 m_d 默认哨兵，isNull()==true，不分配像素。
        return;
    }
    PkImageData data;
    data.width = width;
    data.height = height;
    data.format = static_cast<int>(format);
    int depthBits = depthTable(format);
    data.bytesPerLine = ((width * depthBits + 31) / 32) * 4;
    data.pixels.resize(static_cast<size_t>(data.bytesPerLine) * static_cast<size_t>(height));
    m_d = PkArrayData<PkImageData>(std::move(data));
}

PkImage::PkImage(const PkSize &size, Format format)
    : PkImage(size.width(), size.height(), format)
{
}

PkImage::Format PkImage::format() const
{
    return static_cast<Format>(m_d.PkConst().format);
}

int PkImage::width() const { return m_d.PkConst().width; }
int PkImage::height() const { return m_d.PkConst().height; }
PkSize PkImage::size() const { return PkSize(width(), height()); }
PkRect PkImage::rect() const { return PkRect(0, 0, width(), height()); }

bool PkImage::isNull() const
{
    return width() <= 0 || height() <= 0 || format() == Format_Invalid;
}

int PkImage::depth() const { return depthTable(format()); }

int PkImage::bytesPerLine() const { return m_d.PkConst().bytesPerLine; }

long long PkImage::sizeInBytes() const
{
    return static_cast<long long>(bytesPerLine()) * static_cast<long long>(height());
}

int PkImage::colorCount() const
{
    Format f = format();
    if (f == Format_Mono || f == Format_MonoLSB) {
        return 2;
    }
    if (f == Format_Indexed8) {
        return static_cast<int>(m_d.PkConst().colorTable.size());
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Task 2：像素访问与写入 API
// ---------------------------------------------------------------------------

uint8_t *PkImage::scanLine(int y)
{
    PkImageData &d = m_d.PkMut();
    if (y < 0 || y >= d.height) {
        return nullptr;
    }
    return rowPtrMut(d, y);
}

const uint8_t *PkImage::constScanLine(int y) const
{
    const PkImageData &d = m_d.PkConst();
    if (y < 0 || y >= d.height) {
        return nullptr;
    }
    return rowPtr(d, y);
}

// 修复轮 1：const scanLine 重载，直接转发 constScanLine——绝不能转发给非 const
// scanLine()（那会经 PkMut() 触发 detach，破坏探针第 4 组的硬约束）。真 Qt
// QImage 里 const scanLine(int) const 与 constScanLine 逐字节是同一个函数
// （qimage.h:226 vs 227），这里用转发保住同一语义。
const uint8_t *PkImage::scanLine(int y) const
{
    return constScanLine(y);
}

uint8_t *PkImage::bits()
{
    return m_d.PkMut().pixels.data();
}

const uint8_t *PkImage::constBits() const
{
    return m_d.PkConst().pixels.data();
}

// 修复轮 1：const bits 重载，直接转发 constBits（理由同 const scanLine）。
const uint8_t *PkImage::bits() const
{
    return constBits();
}

uint32_t PkImage::pixel(int x, int y) const
{
    const PkImageData &d = m_d.PkConst();
    if (!coordInBounds(d, x, y)) {
        return 0;
    }
    return rawPixelArgb(d, format(), x, y);
}

void PkImage::setPixel(int x, int y, uint32_t indexOrRgb)
{
    // 越界坐标先用只读路径判掉（不触发 detach）：一次不会真正写入的调用不该
    // 逼共享缓冲区分裂——对齐真 Qt QImage::setPixel() 的顺序（先判界后 detach）。
    if (!coordInBounds(m_d.PkConst(), x, y)) {
        return;
    }
    PkImageData &d = m_d.PkMut();
    writeRawPixelArgb(d, static_cast<Format>(d.format), x, y, indexOrRgb);
}

uint32_t PkImage::pixelColor(int x, int y) const
{
    // 简化决策见 PkImage.h：没有 PkColor，直接复用 pixel() 的 ARGB32 打包值。
    return pixel(x, y);
}

void PkImage::setPixelColor(int x, int y, uint32_t argb)
{
    Format f = format();
    if (f == Format_Mono || f == Format_MonoLSB || f == Format_Indexed8) {
        // 对齐真 Qt：setPixelColor() 在索引格式上是 no-op（真 Qt 会 warning）。
        // setPixel() 才是索引格式的写入口（value 是颜色表索引）。
        return;
    }
    setPixel(x, y, argb);
}

int PkImage::pixelIndex(int x, int y) const
{
    const PkImageData &d = m_d.PkConst();
    if (!coordInBounds(d, x, y)) {
        return 0;
    }
    return rawPixelIndex(d, format(), x, y);
}

void PkImage::fill(uint32_t value)
{
    PkImageData &d = m_d.PkMut();
    Format f = static_cast<Format>(d.format);
    for (int y = 0; y < d.height; ++y) {
        for (int x = 0; x < d.width; ++x) {
            writeRawPixelArgb(d, f, x, y, value);
        }
    }
}

void PkImage::fill(Qt::GlobalColor color)
{
    fill(globalColorToArgb(color));
}

std::vector<uint32_t> PkImage::colorTable() const
{
    const std::vector<uint32_t> &table = m_d.PkConst().colorTable;
    Format f = format();
    if (table.empty() && (f == Format_Mono || f == Format_MonoLSB)) {
        // 未显式 setColorTable 的 Mono/MonoLSB 空表：真 Qt 的 QImage(w,h,
        // Format_Mono) 构造时会自动填充 colorTable = [0xFF000000, 0xFFFFFFFF]
        // （黑、白）。这里合成同样的默认表返回，与 lookupColorTable() 的兜底
        // 默认值（index0 黑 / index1 白）以及 colorCount() 硬返回 2 三处自洽。
        return {0xFF000000u, 0xFFFFFFFFu};
    }
    return table;
}

void PkImage::setColorTable(const std::vector<uint32_t> &colors)
{
    m_d.PkMut().colorTable = colors;
}

void PkImage::setColorCount(int colorCount)
{
    PkImageData &d = m_d.PkMut();
    size_t n = colorCount > 0 ? static_cast<size_t>(colorCount) : 0;
    d.colorTable.resize(n, 0u);
}

uint32_t PkImage::color(int index) const
{
    const PkImageData &d = m_d.PkConst();
    if (index >= 0 && static_cast<size_t>(index) < d.colorTable.size()) {
        return d.colorTable[static_cast<size_t>(index)];
    }
    // 未显式 setColorTable 的 Mono/MonoLSB 空表：委托 lookupColorTable() 合成
    // 黑/白默认值（index0=0xFF000000、index1=0xFFFFFFFF），与真 Qt 自动填充的
    // 默认表一致——消除 color(i) 与 pixel() 路径（lookupColorTable）之间在空表
    // 时各自为政的内部不一致。Indexed8 与其它格式仍按越界读确定性返回 0。
    Format f = format();
    if (d.colorTable.empty() && (f == Format_Mono || f == Format_MonoLSB)) {
        return lookupColorTable(d, f, index);
    }
    return 0;
}

void PkImage::setColor(int index, uint32_t argbColor)
{
    if (index < 0) {
        return;
    }
    PkImageData &d = m_d.PkMut();
    size_t idx = static_cast<size_t>(index);
    if (idx >= d.colorTable.size()) {
        d.colorTable.resize(idx + 1, 0u);
    }
    d.colorTable[idx] = argbColor;
}

bool PkImage::allGray() const
{
    // 语义逐字对齐真 Qt QImage::allGray()（qimage.cpp:2680-2745，5.15）——由
    // oracle 对拍逼出（原实现对 Mono/MonoLSB/Alpha8 无脑返回 true，与 Qt 分家）：
    //   · Grayscale8/Grayscale16：本身就是灰度语义，恒 true。
    //   · Alpha8：纯 alpha 通道，不是灰度，返回 false。
    //   · Mono/MonoLSB/Indexed8：逐项检查颜色表 qIsGray(colorTable[i])，全灰才
    //     true，任一非灰返回 false（不是"索引格式就恒灰"）。
    //   · 32bpp 直接色与其余格式：逐像素 qIsGray（R==G==B），走 rawPixelArgb。
    Format f = format();
    switch (f) {
    case Format_Grayscale8:
    case Format_Grayscale16:
        return true;
    case Format_Alpha8:
        return false;
    case Format_Mono:
    case Format_MonoLSB:
    case Format_Indexed8: {
        // 颜色表逐项判灰。Mono/MonoLSB 未显式 setColorTable 时 colorTable 为空，
        // 空表天然全灰（与 Qt 默认黑白表同是灰度）——与 lookupColorTable 的兜底
        // 默认不一致是两码事：这里判的是"颜色是否都是灰"，不读像素索引。
        const std::vector<uint32_t> &table = m_d.PkConst().colorTable;
        for (uint32_t c : table) {
            if (argbRed(c) != argbGreen(c) || argbGreen(c) != argbBlue(c)) {
                return false;
            }
        }
        return true;
    }
    default:
        break;
    }
    const PkImageData &d = m_d.PkConst();
    for (int y = 0; y < d.height; ++y) {
        for (int x = 0; x < d.width; ++x) {
            uint32_t c = rawPixelArgb(d, f, x, y);
            if (argbRed(c) != argbGreen(c) || argbGreen(c) != argbBlue(c)) {
                return false;
            }
        }
    }
    return true;
}

long PkImage::PkUseCount() const noexcept
{
    return m_d.PkUseCount();
}

bool PkImage::PkIsSharedWith(const PkImage &other) const noexcept
{
    return m_d.PkIsSharedWith(other.m_d);
}

// ---------------------------------------------------------------------------
// Task 3：格式转换、派生操作
// ---------------------------------------------------------------------------

PkImage PkImage::copy() const
{
    // 探针第 7 组：无条件深拷贝，即使原本没有共享者也强制产生新分配。
    // PkArrayData<C> 的 `explicit PkArrayData(C init)` 构造函数按「有没有显式
    // 给 C」判独占，不看内容——传一份 PkImageData 的拷贝进去就是新的
    // make_shared，全新独立引用计数（pk/container/PkArrayData.h:140）。
    // null 的 copy() 结果仍是 null：PkConst() 在哨兵状态下的默认值就是 null
    // 状态（width=height=format=0），直接拷贝即可，不需要特判。
    PkImage result;
    result.m_d = PkArrayData<PkImageData>(PkImageData(m_d.PkConst()));
    return result;
}

PkImage PkImage::convertToFormat(Format newFormat) const
{
    // 探针第 6 组：同格式共享，不拷贝。
    if (newFormat == format()) {
        return *this;
    }

    PkImage result(width(), height(), newFormat);
    if (result.isNull()) {
        // 源本身是 null，或目标格式本身构造不出合法图像（Format_Invalid）。
        return result;
    }

    const PkImageData &srcData = m_d.PkConst();
    PkImageData &dstData = result.m_d.PkMut();
    dstData.devicePixelRatio = srcData.devicePixelRatio;

    // 转换到索引格式（Indexed8/Mono/MonoLSB）需要调色板生成算法（颜色量化/
    // 最近色匹配），真实调用点用量表没有覆盖这个方向（判据①，一项不多）。
    // rawPixelArgb() 读回的是已经查过表的 ARGB32 颜色值，而 writeRawPixelArgb()
    // 对索引格式的语义是把 value 当**颜色表索引**写入（与 setPixel() 同一套
    // 约定，见 Task 2）——两者直接拼起来会把 ARGB 的低 8 位错当索引写进去，
    // 是真实的 bug，不是可以忽略的边角。这里选择保持构造时的零初始化状态
    // （全部索引 0，颜色表为空）：确定性、不崩溃，不产出错误数据。
    Format fmt = newFormat;
    if (fmt == Format_Indexed8 || fmt == Format_Mono || fmt == Format_MonoLSB) {
        return result;
    }

    int w = width();
    int h = height();
    Format srcFormat = format();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint32_t argb = rawPixelArgb(srcData, srcFormat, x, y);
            writeRawPixelArgb(dstData, fmt, x, y, argb);
        }
    }
    return result;
}

// Fix round 1（评审后追加）：调用方指定调色板的重载。算法与理由见
// nearestColorIndex() 的注释；这里只负责逐像素求 ARGB32 值、查最近色索引、
// 把索引写入目标像素——完全复用 Task 2 的 rawPixelArgb/writeRawPixelIndex，
// 不重新写一套像素读写逻辑。
PkImage PkImage::convertToFormat(Format targetFormat, const std::vector<uint32_t> &colorTable) const
{
    PkImage result(width(), height(), targetFormat);
    if (result.isNull()) {
        // 源本身是 null，或目标格式构造不出合法图像。
        return result;
    }

    PkImageData &dstData = result.m_d.PkMut();
    dstData.devicePixelRatio = m_d.PkConst().devicePixelRatio;
    // 目标图像的 colorTable 就是调用方传入的 colorTable 原样拷贝（brief 明文
    // 要求），与像素是否真的全部映射到表内某项无关。
    dstData.colorTable = colorTable;

    if (colorTable.empty()) {
        // 空调色板：没有任何候选项可匹配，不编造数据——保持 PkImage(width,
        // height, targetFormat) 构造时的零初始化索引状态（全部像素索引 0），
        // 确定性、不崩溃，这是 brief 明确要求判断的边界情况里最安全的选择。
        return result;
    }

    const PkImageData &srcData = m_d.PkConst();
    Format srcFormat = format();
    int w = width();
    int h = height();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint32_t argb = rawPixelArgb(srcData, srcFormat, x, y);
            int idx = nearestColorIndex(argb, colorTable);
            writeRawPixelIndex(dstData, targetFormat, x, y, static_cast<uint32_t>(idx));
        }
    }
    return result;
}

void PkImage::convertTo(Format newFormat)
{
    *this = convertToFormat(newFormat);
}

qreal PkImage::devicePixelRatio() const
{
    return m_d.PkConst().devicePixelRatio;
}

void PkImage::setDevicePixelRatio(qreal scaleFactor)
{
    m_d.PkMut().devicePixelRatio = scaleFactor;
}

// 结论 1：scaled() 是 transformed(PkTransform::fromScale(sx, sy), mode) 的一层
// 包装，不是独立算法（探针实测逐像素相等）。
PkImage PkImage::scaled(const PkSize &targetSize, Qt::AspectRatioMode aspectMode, Qt::TransformationMode mode) const
{
    if (isNull()) {
        // 源尺寸为 0 时避免下面 newSize.width()/width() 的比例除法产生
        // inf/nan——探针没有覆盖这个退化输入，是防御性处理：inf 传进
        // PkTransform 会在 mapRect 内部的 qRound(inf) 上触发未定义行为
        // （浮点转 int 越界是 UB，与 -fwrapv 只挡整数溢出是两类问题）。
        return PkImage();
    }
    PkSize newSize = size().scaled(targetSize, aspectMode);
    // 探针确认：结果维度被 clamp 到至少 1（KeepAspectRatio 压扁到 0 的情形）。
    newSize = PkSize(std::max(newSize.width(), 1), std::max(newSize.height(), 1));
    if (newSize == size()) {
        // 探针确认：相同尺寸直接共享（same-ptr=1），不重新分配。
        return *this;
    }
    PkTransform t = PkTransform::fromScale(qreal(newSize.width()) / width(), qreal(newSize.height()) / height());
    return transformed(t, mode);
}

PkImage PkImage::transformed(const PkTransform &matrix, Qt::TransformationMode mode) const
{
    // 结论 3 旁注：identity 变换直接共享短路，跳过整个映射循环（探针确认
    // same-ptr=1）。放在最前面：null 图像上的 identity 变换也应该直接共享
    // 自身，不需要单独判 isNull()。
    if (matrix.isIdentity()) {
        return *this;
    }
    if (isNull()) {
        return PkImage();
    }

    // 结论 2：目标尺寸 = transform.mapRect(PkRect(0,0,width,height))，整数矩形
    // 映射，PkTransform::mapRect(const PkRect&) 是 R-03 VERIFIED 交付，直接复用。
    PkRect boundingRect = matrix.mapRect(rect());
    PkImage result(boundingRect.width(), boundingRect.height(), format());
    if (result.isNull()) {
        // boundingRect 退化（宽或高 <=0）时 PkImage 构造函数本身就返回 null，
        // 不需要额外判空分支。
        return result;
    }

    const PkImageData &srcData = m_d.PkConst();
    PkImageData &dstData = result.m_d.PkMut();
    // 结论 4：devicePixelRatio 原样透传，不重置为 1.0（探针确认）。
    dstData.devicePixelRatio = srcData.devicePixelRatio;

    Format fmt = format();
    bool isIndexedFmt = (fmt == Format_Indexed8 || fmt == Format_Mono || fmt == Format_MonoLSB);
    if (isIndexedFmt) {
        // 同一次 transformed() 内格式不变，索引配色表原样带过去，逐像素只搬
        // 索引，不经 ARGB 往返——往返会把索引误当颜色值，与 convertToFormat()
        // 里同一个理由。
        dstData.colorTable = srcData.colorTable;
    }

    // 矩阵不可逆（det==0）时 PkTransform::inverted() 已经确定性地退回单位阵
    // （见 PkTransform.h 类头注释），不需要额外分支——退化矩阵下退回恒等映射
    // 是 PkTransform 自己的既有约定，不是本函数新引入的行为；不可逆标志无需
    // 读取，用默认参数（nullptr）。
    PkTransform inv = matrix.inverted();

    const int dstWidth = boundingRect.width();
    const int dstHeight = boundingRect.height();
    const int srcWidth = width();
    const int srcHeight = height();

    // Smooth 模式对索引格式退化成最近邻：混合调色板索引没有良定义的颜色语义
    // （岔路 B 范围内的自定义决策，不追求跟 Qt 位对齐）。
    const bool useNearest = (mode == Qt::FastTransformation) || isIndexedFmt;

    for (int dstY = 0; dstY < dstHeight; ++dstY) {
        for (int dstX = 0; dstX < dstWidth; ++dstX) {
            // 结论 3：像素中心逆映射后向下取整。
            PkPointF srcPoint = inv.map(PkPointF(
                dstX + boundingRect.x() + 0.5,
                dstY + boundingRect.y() + 0.5));

            if (useNearest) {
                int srcX = static_cast<int>(std::floor(srcPoint.x()));
                int srcY = static_cast<int>(std::floor(srcPoint.y()));
                if (srcX >= 0 && srcX < srcWidth && srcY >= 0 && srcY < srcHeight) {
                    if (isIndexedFmt) {
                        int idx = rawPixelIndex(srcData, fmt, srcX, srcY);
                        writeRawPixelIndex(dstData, fmt, dstX, dstY, static_cast<uint32_t>(idx));
                    } else {
                        uint32_t argb = rawPixelArgb(srcData, fmt, srcX, srcY);
                        writeRawPixelArgb(dstData, fmt, dstX, dstY, argb);
                    }
                }
                // else：越界区域保持构造时的零初始化（探针确认输出 0x00000000，
                // 全透明黑；索引格式则是索引 0），不用写。
                continue;
            }

            // Smooth（双线性）：把「像素中心」定义为整数坐标 + 0.5（与最近邻
            // 分支同一套坐标系），取周围 4 个源像素按小数部分线性加权。
            double fx = srcPoint.x() - 0.5;
            double fy = srcPoint.y() - 0.5;
            int x0 = static_cast<int>(std::floor(fx));
            int y0 = static_cast<int>(std::floor(fy));
            double wx = fx - x0;
            double wy = fy - y0;
            int x0c = clampCoord(x0, srcWidth);
            int x1c = clampCoord(x0 + 1, srcWidth);
            int y0c = clampCoord(y0, srcHeight);
            int y1c = clampCoord(y0 + 1, srcHeight);
            uint32_t c00 = rawPixelArgb(srcData, fmt, x0c, y0c);
            uint32_t c10 = rawPixelArgb(srcData, fmt, x1c, y0c);
            uint32_t c01 = rawPixelArgb(srcData, fmt, x0c, y1c);
            uint32_t c11 = rawPixelArgb(srcData, fmt, x1c, y1c);
            writeRawPixelArgb(dstData, fmt, dstX, dstY, bilerpArgb(c00, c10, c01, c11, wx, wy));
        }
    }

    return result;
}

bool PkImage::operator==(const PkImage &other) const
{
    // 共享指针相等是短路优化：两个默认构造/null 图像都指向 PkArrayData 的
    // 进程内共享空哨兵，天然共享，直接 true——对应探针第 5 组「两个 null 都
    // 是 true」。
    if (PkIsSharedWith(other)) {
        return true;
    }
    // 不共享时逐字节比较像素内容与格式/尺寸。不比较 devicePixelRatio——对齐
    // 真 Qt QImage::operator== 的语义（探针第 5 组旁注）。
    const PkImageData &a = m_d.PkConst();
    const PkImageData &b = other.m_d.PkConst();
    return a.format == b.format
        && a.width == b.width
        && a.height == b.height
        && a.bytesPerLine == b.bytesPerLine
        && a.pixels == b.pixels
        && a.colorTable == b.colorTable;
}

bool PkImage::operator!=(const PkImage &other) const
{
    return !(*this == other);
}
