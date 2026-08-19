#include "PkImage.h"

#include <cassert>
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

uint8_t *PkImage::bits()
{
    return m_d.PkMut().pixels.data();
}

const uint8_t *PkImage::constBits() const
{
    return m_d.PkConst().pixels.data();
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
    return m_d.PkConst().colorTable;
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
    if (index < 0 || static_cast<size_t>(index) >= d.colorTable.size()) {
        return 0;
    }
    return d.colorTable[static_cast<size_t>(index)];
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
    Format f = format();
    if (f == Format_Grayscale8 || f == Format_Grayscale16 || f == Format_Alpha8
        || f == Format_Mono || f == Format_MonoLSB) {
        // 本身就是灰度/单色语义，直接 true（探针第13组：按定义实现，未专门探测）。
        return true;
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
