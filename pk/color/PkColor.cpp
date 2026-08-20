// PkColor.cpp —— 逐字照抄真 Qt 5.15.7 qcolor.cpp 的实现语义。
//
// 对照源：/tmp/qcolor515.cpp（从 ci-env _install/include/QtGui/5.15.7/QtGui/
// qcolor.cpp 提取），关键函数行号标在各函数注释里。所有 8-bit getter 走
// qt_div_257，所有 HSV/HSL 转换走 qreal/65535 + qRound——这两条是「对拍
// mismatch=0」的地基，不能用 brief Step 3 的整数简化版（那版在边界上与真 Qt
// 分家，探针 fromHsvF(0.333,1,1)=(1,255,0) 就是 16-bit 取整的证据）。
//
// 内部一律用 pkQtFuzzy* 而非 qFuzzy*：PkGlobal.h 在「pk/test 垫片先落地」的
// 路径上会把 qFuzzyCompare/qFuzzyIsNull 变成宏（→ pkFuzzy*），pkQt* 是唯一
// 无条件定义的公式名（PkGlobal.h 注释原话「内部一律走 pkQt*，宏改写不到」）。

#include "PkColor.h"

#include <algorithm>   // std::lower_bound
#include <cstdio>      // std::snprintf
#include <cstring>     // std::strlen / std::strcmp
#include <string>      // std::string（setNamedColor(PkString) 的中间形态）

namespace {

constexpr int kUShortMax = 65535;
constexpr int kUCharMax = 255;

// qglobal.h 的 qt_div_257：16-bit → 8-bit。
constexpr int qt_div_257(int x) { return (x + 128) / 257; }

// qcolor.h isRgbaValid：全部落在 [0,255]。
constexpr bool isRgbaValid(int r, int g, int b, int a)
{ return (quint32(r) <= 255) && (quint32(g) <= 255) && (quint32(b) <= 255) && (quint32(a) <= 255); }

// QRgb 位域辅助（qcolor.h qRed/qGreen/qBlue/qAlpha 同款）。
constexpr quint32 pkRgb(int r, int g, int b)
{ return (0xffu << 24) | ((quint32(r) & 0xff) << 16) | ((quint32(g) & 0xff) << 8) | (quint32(b) & 0xff); }
constexpr quint32 pkRgba(int r, int g, int b, int a)
{ return ((quint32(a) & 0xff) << 24) | ((quint32(r) & 0xff) << 16) | ((quint32(g) & 0xff) << 8) | (quint32(b) & 0xff); }
constexpr int pkRed(quint32 c) { return int((c >> 16) & 0xff); }
constexpr int pkGreen(quint32 c) { return int((c >> 8) & 0xff); }
constexpr int pkBlue(quint32 c) { return int(c & 0xff); }
constexpr int pkAlpha(quint32 c) { return int((c >> 24) & 0xff); }

// QCOLOR_INT_RANGE_CHECK（qcolor.cpp:606）：截断，不置无效。
void intRangeCheck(int &v)
{ if (v < 0 || v > 255) v = qMax(0, qMin(v, 255)); }

// QCOLOR_REAL_RANGE_CHECK（qcolor.cpp:614）：截断到 [0,1]，不置无效。
void realRangeCheck(qreal &v)
{ if (v < qreal(0.0) || v > qreal(1.0)) v = qMax(qreal(0.0), qMin(v, qreal(1.0))); }

// QtMiscUtils::fromHex（qcolor.cpp 用的单字符 hex 解码，-1 表示非法）。
constexpr int fromHex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// hex2int（qcolor.cpp:62）。返回 -1 表示非法。
int hex2int(const char *s, int n)
{
    if (n < 0) return -1;
    int result = 0;
    for (; n > 0; --n) {
        result = result * 16;
        const int h = fromHex(*s++);
        if (h < 0) return -1;
        result += h;
    }
    return result;
}

// get_hex_rgb（qcolor.cpp:79）：#RGB/#RRGGBB/#AARRGGBB/#RRRGGGBBB/#RRRRGGGGBBBB。
// 与 Qt 不同的是把 QRgba64 输出拆成 4 个 quint16（本类没有 QRgba64 值类型）。
bool getHexRgb(const char *name, size_t len, quint16 *a, quint16 *r, quint16 *g, quint16 *b)
{
    if (name[0] != '#') return false;
    ++name; --len;
    int ra, rr, rg, rb;
    ra = 65535;
    if (len == 12) {
        rr = hex2int(name + 0, 4);
        rg = hex2int(name + 4, 4);
        rb = hex2int(name + 8, 4);
    } else if (len == 9) {
        rr = hex2int(name + 0, 3);
        rg = hex2int(name + 3, 3);
        rb = hex2int(name + 6, 3);
        if (rr == -1 || rg == -1 || rb == -1) return false;
        rr = (rr << 4) | (rr >> 8);
        rg = (rg << 4) | (rg >> 8);
        rb = (rb << 4) | (rb >> 8);
    } else if (len == 8) {
        ra = hex2int(name + 0, 2) * 0x101;
        rr = hex2int(name + 2, 2) * 0x101;
        rg = hex2int(name + 4, 2) * 0x101;
        rb = hex2int(name + 6, 2) * 0x101;
    } else if (len == 6) {
        rr = hex2int(name + 0, 2) * 0x101;
        rg = hex2int(name + 2, 2) * 0x101;
        rb = hex2int(name + 4, 2) * 0x101;
    } else if (len == 3) {
        rr = hex2int(name + 0, 1) * 0x1111;
        rg = hex2int(name + 1, 1) * 0x1111;
        rb = hex2int(name + 2, 1) * 0x1111;
    } else {
        rr = rg = rb = -1;
    }
    if ((quint32)rr > 65535 || (quint32)rg > 65535 || (quint32)rb > 65535 || (quint32)ra > 65535)
        return false;
    *a = quint16(ra); *r = quint16(rr); *g = quint16(rg); *b = quint16(rb);
    return true;
}

// rgbTbl（qcolor.cpp:156-307）——SVG 1.0 命名色 + transparent，**逐字照抄**
// 顺序与取值都不能动：get_named_rgb_no_space 用 std::lower_bound 二分，顺序错
// 了二分结果就错；"green"=(0,128,0) 与 Qt::green=(0,255,0) 不同是有意的
// （QColor 文档原话：SVG 名与 Qt::GlobalColor 枚举不是同一组颜色）。
struct RGBData { const char name[21]; quint32 value; };
constexpr quint32 kRgbMacro(int r, int g, int b) { return pkRgb(r, g, b); }

const RGBData kRgbTbl[] = {
    { "aliceblue", kRgbMacro(240, 248, 255) },
    { "antiquewhite", kRgbMacro(250, 235, 215) },
    { "aqua", kRgbMacro( 0, 255, 255) },
    { "aquamarine", kRgbMacro(127, 255, 212) },
    { "azure", kRgbMacro(240, 255, 255) },
    { "beige", kRgbMacro(245, 245, 220) },
    { "bisque", kRgbMacro(255, 228, 196) },
    { "black", kRgbMacro( 0, 0, 0) },
    { "blanchedalmond", kRgbMacro(255, 235, 205) },
    { "blue", kRgbMacro( 0, 0, 255) },
    { "blueviolet", kRgbMacro(138, 43, 226) },
    { "brown", kRgbMacro(165, 42, 42) },
    { "burlywood", kRgbMacro(222, 184, 135) },
    { "cadetblue", kRgbMacro( 95, 158, 160) },
    { "chartreuse", kRgbMacro(127, 255, 0) },
    { "chocolate", kRgbMacro(210, 105, 30) },
    { "coral", kRgbMacro(255, 127, 80) },
    { "cornflowerblue", kRgbMacro(100, 149, 237) },
    { "cornsilk", kRgbMacro(255, 248, 220) },
    { "crimson", kRgbMacro(220, 20, 60) },
    { "cyan", kRgbMacro( 0, 255, 255) },
    { "darkblue", kRgbMacro( 0, 0, 139) },
    { "darkcyan", kRgbMacro( 0, 139, 139) },
    { "darkgoldenrod", kRgbMacro(184, 134, 11) },
    { "darkgray", kRgbMacro(169, 169, 169) },
    { "darkgreen", kRgbMacro( 0, 100, 0) },
    { "darkgrey", kRgbMacro(169, 169, 169) },
    { "darkkhaki", kRgbMacro(189, 183, 107) },
    { "darkmagenta", kRgbMacro(139, 0, 139) },
    { "darkolivegreen", kRgbMacro( 85, 107, 47) },
    { "darkorange", kRgbMacro(255, 140, 0) },
    { "darkorchid", kRgbMacro(153, 50, 204) },
    { "darkred", kRgbMacro(139, 0, 0) },
    { "darksalmon", kRgbMacro(233, 150, 122) },
    { "darkseagreen", kRgbMacro(143, 188, 143) },
    { "darkslateblue", kRgbMacro( 72, 61, 139) },
    { "darkslategray", kRgbMacro( 47, 79, 79) },
    { "darkslategrey", kRgbMacro( 47, 79, 79) },
    { "darkturquoise", kRgbMacro( 0, 206, 209) },
    { "darkviolet", kRgbMacro(148, 0, 211) },
    { "deeppink", kRgbMacro(255, 20, 147) },
    { "deepskyblue", kRgbMacro( 0, 191, 255) },
    { "dimgray", kRgbMacro(105, 105, 105) },
    { "dimgrey", kRgbMacro(105, 105, 105) },
    { "dodgerblue", kRgbMacro( 30, 144, 255) },
    { "firebrick", kRgbMacro(178, 34, 34) },
    { "floralwhite", kRgbMacro(255, 250, 240) },
    { "forestgreen", kRgbMacro( 34, 139, 34) },
    { "fuchsia", kRgbMacro(255, 0, 255) },
    { "gainsboro", kRgbMacro(220, 220, 220) },
    { "ghostwhite", kRgbMacro(248, 248, 255) },
    { "gold", kRgbMacro(255, 215, 0) },
    { "goldenrod", kRgbMacro(218, 165, 32) },
    { "gray", kRgbMacro(128, 128, 128) },
    { "green", kRgbMacro( 0, 128, 0) },
    { "greenyellow", kRgbMacro(173, 255, 47) },
    { "grey", kRgbMacro(128, 128, 128) },
    { "honeydew", kRgbMacro(240, 255, 240) },
    { "hotpink", kRgbMacro(255, 105, 180) },
    { "indianred", kRgbMacro(205, 92, 92) },
    { "indigo", kRgbMacro( 75, 0, 130) },
    { "ivory", kRgbMacro(255, 255, 240) },
    { "khaki", kRgbMacro(240, 230, 140) },
    { "lavender", kRgbMacro(230, 230, 250) },
    { "lavenderblush", kRgbMacro(255, 240, 245) },
    { "lawngreen", kRgbMacro(124, 252, 0) },
    { "lemonchiffon", kRgbMacro(255, 250, 205) },
    { "lightblue", kRgbMacro(173, 216, 230) },
    { "lightcoral", kRgbMacro(240, 128, 128) },
    { "lightcyan", kRgbMacro(224, 255, 255) },
    { "lightgoldenrodyellow", kRgbMacro(250, 250, 210) },
    { "lightgray", kRgbMacro(211, 211, 211) },
    { "lightgreen", kRgbMacro(144, 238, 144) },
    { "lightgrey", kRgbMacro(211, 211, 211) },
    { "lightpink", kRgbMacro(255, 182, 193) },
    { "lightsalmon", kRgbMacro(255, 160, 122) },
    { "lightseagreen", kRgbMacro( 32, 178, 170) },
    { "lightskyblue", kRgbMacro(135, 206, 250) },
    { "lightslategray", kRgbMacro(119, 136, 153) },
    { "lightslategrey", kRgbMacro(119, 136, 153) },
    { "lightsteelblue", kRgbMacro(176, 196, 222) },
    { "lightyellow", kRgbMacro(255, 255, 224) },
    { "lime", kRgbMacro( 0, 255, 0) },
    { "limegreen", kRgbMacro( 50, 205, 50) },
    { "linen", kRgbMacro(250, 240, 230) },
    { "magenta", kRgbMacro(255, 0, 255) },
    { "maroon", kRgbMacro(128, 0, 0) },
    { "mediumaquamarine", kRgbMacro(102, 205, 170) },
    { "mediumblue", kRgbMacro( 0, 0, 205) },
    { "mediumorchid", kRgbMacro(186, 85, 211) },
    { "mediumpurple", kRgbMacro(147, 112, 219) },
    { "mediumseagreen", kRgbMacro( 60, 179, 113) },
    { "mediumslateblue", kRgbMacro(123, 104, 238) },
    { "mediumspringgreen", kRgbMacro( 0, 250, 154) },
    { "mediumturquoise", kRgbMacro( 72, 209, 204) },
    { "mediumvioletred", kRgbMacro(199, 21, 133) },
    { "midnightblue", kRgbMacro( 25, 25, 112) },
    { "mintcream", kRgbMacro(245, 255, 250) },
    { "mistyrose", kRgbMacro(255, 228, 225) },
    { "moccasin", kRgbMacro(255, 228, 181) },
    { "navajowhite", kRgbMacro(255, 222, 173) },
    { "navy", kRgbMacro( 0, 0, 128) },
    { "oldlace", kRgbMacro(253, 245, 230) },
    { "olive", kRgbMacro(128, 128, 0) },
    { "olivedrab", kRgbMacro(107, 142, 35) },
    { "orange", kRgbMacro(255, 165, 0) },
    { "orangered", kRgbMacro(255, 69, 0) },
    { "orchid", kRgbMacro(218, 112, 214) },
    { "palegoldenrod", kRgbMacro(238, 232, 170) },
    { "palegreen", kRgbMacro(152, 251, 152) },
    { "paleturquoise", kRgbMacro(175, 238, 238) },
    { "palevioletred", kRgbMacro(219, 112, 147) },
    { "papayawhip", kRgbMacro(255, 239, 213) },
    { "peachpuff", kRgbMacro(255, 218, 185) },
    { "peru", kRgbMacro(205, 133, 63) },
    { "pink", kRgbMacro(255, 192, 203) },
    { "plum", kRgbMacro(221, 160, 221) },
    { "powderblue", kRgbMacro(176, 224, 230) },
    { "purple", kRgbMacro(128, 0, 128) },
    { "red", kRgbMacro(255, 0, 0) },
    { "rosybrown", kRgbMacro(188, 143, 143) },
    { "royalblue", kRgbMacro( 65, 105, 225) },
    { "saddlebrown", kRgbMacro(139, 69, 19) },
    { "salmon", kRgbMacro(250, 128, 114) },
    { "sandybrown", kRgbMacro(244, 164, 96) },
    { "seagreen", kRgbMacro( 46, 139, 87) },
    { "seashell", kRgbMacro(255, 245, 238) },
    { "sienna", kRgbMacro(160, 82, 45) },
    { "silver", kRgbMacro(192, 192, 192) },
    { "skyblue", kRgbMacro(135, 206, 235) },
    { "slateblue", kRgbMacro(106, 90, 205) },
    { "slategray", kRgbMacro(112, 128, 144) },
    { "slategrey", kRgbMacro(112, 128, 144) },
    { "snow", kRgbMacro(255, 250, 250) },
    { "springgreen", kRgbMacro( 0, 255, 127) },
    { "steelblue", kRgbMacro( 70, 130, 180) },
    { "tan", kRgbMacro(210, 180, 140) },
    { "teal", kRgbMacro( 0, 128, 128) },
    { "thistle", kRgbMacro(216, 191, 216) },
    { "tomato", kRgbMacro(255, 99, 71) },
    { "transparent", 0 },
    { "turquoise", kRgbMacro( 64, 224, 208) },
    { "violet", kRgbMacro(238, 130, 238) },
    { "wheat", kRgbMacro(245, 222, 179) },
    { "white", kRgbMacro(255, 255, 255) },
    { "whitesmoke", kRgbMacro(245, 245, 245) },
    { "yellow", kRgbMacro(255, 255, 0) },
    { "yellowgreen", kRgbMacro(154, 205, 50) }
};
const int kRgbTblSize = int(sizeof(kRgbTbl) / sizeof(RGBData));

// qcolor.cpp:308 的两个 operator<（std::lower_bound 比较用）。
inline bool operator<(const char *name, const RGBData &data)
{ return std::strcmp(name, data.name) < 0; }
inline bool operator<(const RGBData &data, const char *name)
{ return std::strcmp(data.name, name) < 0; }

// get_named_rgb_no_space（qcolor.cpp:310）。
bool getNamedRgbNoSpace(const char *nameNoSpace, quint32 *rgb)
{
    const RGBData *r = std::lower_bound(kRgbTbl, kRgbTbl + kRgbTblSize, nameNoSpace);
    if ((r != kRgbTbl + kRgbTblSize) && !(nameNoSpace < *r)) {
        *rgb = r->value;
        return true;
    }
    return false;
}

// get_named_rgb（qcolor.cpp:327）：去空白 + 转小写 + 二分。
bool getNamedRgb(const char *name, int len, quint32 *rgb)
{
    if (len > 255) return false;
    char nameNoSpace[256];
    int pos = 0;
    for (int i = 0; i < len; ++i) {
        const char c = name[i];
        if (c != '\t' && c != ' ')
            nameNoSpace[pos++] = (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
    }
    nameNoSpace[pos] = 0;
    return getNamedRgbNoSpace(nameNoSpace, rgb);
}

float halfToFloat(quint16 half) noexcept
{
    const quint32 sign = static_cast<quint32>(half & 0x8000u) << 16;
    int exponent = static_cast<int>((half >> 10) & 0x1fu);
    quint32 mantissa = half & 0x03ffu;
    quint32 bits = 0;
    if (exponent == 0) {
        if (mantissa == 0u) {
            bits = sign;
        } else {
            exponent = 1;
            while ((mantissa & 0x0400u) == 0u) {
                mantissa <<= 1;
                --exponent;
            }
            mantissa &= 0x03ffu;
            bits = sign | (static_cast<quint32>(exponent + 112) << 23) | (mantissa << 13);
        }
    } else if (exponent == 0x1f) {
        bits = sign | 0x7f800000u | (mantissa << 13);
    } else {
        bits = sign | (static_cast<quint32>(exponent + 112) << 23) | (mantissa << 13);
    }
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

quint32 roundShiftRight(quint32 value, unsigned int shift) noexcept
{
    const quint32 result = value >> shift;
    const quint32 mask = (quint32(1) << shift) - 1u;
    const quint32 remainder = value & mask;
    const quint32 halfway = quint32(1) << (shift - 1u);
    return result + (remainder > halfway || (remainder == halfway && (result & 1u)) ? 1u : 0u);
}

quint16 floatToHalf(float value) noexcept
{
    quint32 bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const quint16 sign = static_cast<quint16>((bits >> 16) & 0x8000u);
    const quint32 floatExponent = (bits >> 23) & 0xffu;
    const quint32 mantissa = bits & 0x007fffffu;
    if (floatExponent == 0xffu) {
        if (mantissa == 0u) return static_cast<quint16>(sign | 0x7c00u);
        const quint16 payload = static_cast<quint16>(mantissa >> 13);
        return static_cast<quint16>(sign | 0x7c00u | (payload ? payload : 1u));
    }

    const int exponent = static_cast<int>(floatExponent) - 127 + 15;
    if (exponent >= 31) return static_cast<quint16>(sign | 0x7c00u);
    if (exponent <= 0) {
        if (exponent < -10) return sign;
        const quint32 significand = mantissa | 0x00800000u;
        const unsigned int shift = static_cast<unsigned int>(14 - exponent);
        return static_cast<quint16>(sign | roundShiftRight(significand, shift));
    }

    quint32 roundedMantissa = roundShiftRight(mantissa, 13u);
    int roundedExponent = exponent;
    if (roundedMantissa == 0x0400u) {
        roundedMantissa = 0u;
        ++roundedExponent;
        if (roundedExponent >= 31) return static_cast<quint16>(sign | 0x7c00u);
    }
    return static_cast<quint16>(sign | (static_cast<quint16>(roundedExponent) << 10)
                                | static_cast<quint16>(roundedMantissa));
}

} // namespace

// ---------------------------------------------------------------------------
// 构造 / 析构 / 赋值
// ---------------------------------------------------------------------------

// qcolor.h 默认构造：cspec(Invalid), ct(USHRT_MAX,0,0,0,0) —— alpha=65535，
// 所以无效色的 rgba() 仍给出 alpha=255（rgba() 对 Invalid 走 qt_div_257 分支）。
PkColor::PkColor() noexcept
    : cspec(Invalid)
{
    ct.argb.alpha = kUShortMax;
    ct.argb.red = 0; ct.argb.green = 0; ct.argb.blue = 0; ct.argb.pad = 0;
}

// qcolor.h int 构造：越界 → 无效且**所有分量含 alpha 置 0**（与默认构造不同）。
PkColor::PkColor(int r, int g, int b, int a) noexcept
    : cspec(isRgbaValid(r, g, b, a) ? Rgb : Invalid)
{
    ct.argb.alpha = quint16(cspec == Rgb ? a * 0x101 : 0);
    ct.argb.red   = quint16(cspec == Rgb ? r * 0x101 : 0);
    ct.argb.green = quint16(cspec == Rgb ? g * 0x101 : 0);
    ct.argb.blue  = quint16(cspec == Rgb ? b * 0x101 : 0);
    ct.argb.pad   = 0;
}

// qcolor.cpp:680 GlobalColor 构造：20 项表 + setRgb(int,int,int,int)。
// 注意 green=(0,255,0)、darkYellow=(128,128,0) **有效**——这与 SVG 命名色
// "green"=(0,128,0)、"darkYellow" 无效是两回事（brief 探针把它们混写，已登记偏离）。
PkColor::PkColor(Qt::GlobalColor color) noexcept
{
    static const quint32 globalColors[] = {
        pkRgb(255, 255, 255), // color0
        pkRgb(  0,   0,   0), // color1
        pkRgb(  0,   0,   0), // black
        pkRgb(255, 255, 255), // white
        pkRgb(128, 128, 128), // darkGray
        pkRgb(160, 160, 164), // gray
        pkRgb(192, 192, 192), // lightGray
        pkRgb(255,   0,   0), // red
        pkRgb(  0, 255,   0), // green
        pkRgb(  0,   0, 255), // blue
        pkRgb(  0, 255, 255), // cyan
        pkRgb(255,   0, 255), // magenta
        pkRgb(255, 255,   0), // yellow
        pkRgb(128,   0,   0), // darkRed
        pkRgb(  0, 128,   0), // darkGreen
        pkRgb(  0,   0, 128), // darkBlue
        pkRgb(  0, 128, 128), // darkCyan
        pkRgb(128,   0, 128), // darkMagenta
        pkRgb(128, 128,   0), // darkYellow
        pkRgba(0, 0, 0, 0)    // transparent
    };
    const quint32 c = globalColors[(int)color];
    setRgb(pkRed(c), pkGreen(c), pkBlue(c), pkAlpha(c));
}

PkColor::PkColor(const char *name) { setNamedColor(name); }
PkColor::PkColor(const PkString &name) { setNamedColor(name); }

PkColor &PkColor::operator=(Qt::GlobalColor color) noexcept
{ return *this = PkColor(color); }

// ---------------------------------------------------------------------------
// 静态工厂
// ---------------------------------------------------------------------------

// qcolor.cpp:2414 fromRgb(int,int,int,int)：越界 → 返回无效色。
PkColor PkColor::fromRgb(int r, int g, int b, int a)
{
    if (!isRgbaValid(r, g, b, a)) return PkColor();
    PkColor color;
    color.cspec = Rgb;
    color.ct.argb.alpha = quint16(a * 0x101);
    color.ct.argb.red   = quint16(r * 0x101);
    color.ct.argb.green = quint16(g * 0x101);
    color.ct.argb.blue  = quint16(b * 0x101);
    color.ct.argb.pad   = 0;
    return color;
}

// qcolor.cpp:2384 fromRgb(QRgb)：alpha 忽略，置 255。
PkColor PkColor::fromRgb(quint32 rgb) noexcept
{ return fromRgb(pkRed(rgb), pkGreen(rgb), pkBlue(rgb)); }

// qcolor.cpp:2400 fromRgba(QRgb)：含 alpha。
PkColor PkColor::fromRgba(quint32 rgba) noexcept
{ return fromRgb(pkRed(rgba), pkGreen(rgba), pkBlue(rgba), pkAlpha(rgba)); }

// qcolor.cpp:2442 fromRgbF：alpha 越界 → 无效；rgb 越界 → ExtendedRgb。
PkColor PkColor::fromRgbF(qreal r, qreal g, qreal b, qreal a)
{
    if (a < qreal(0.0) || a > qreal(1.0)) return PkColor();
    if (r < qreal(0.0) || r > qreal(1.0)
        || g < qreal(0.0) || g > qreal(1.0)
        || b < qreal(0.0) || b > qreal(1.0)) {
        PkColor color;
        color.cspec = ExtendedRgb;
        color.ct.argbExt.alphaF = (float)a;
        color.ct.argbExt.redF   = (float)r;
        color.ct.argbExt.greenF = (float)g;
        color.ct.argbExt.blueF  = (float)b;
        return color;
    }
    PkColor color;
    color.cspec = Rgb;
    color.ct.argb.alpha = quint16(qRound(a * kUShortMax));
    color.ct.argb.red   = quint16(qRound(r * kUShortMax));
    color.ct.argb.green = quint16(qRound(g * kUShortMax));
    color.ct.argb.blue  = quint16(qRound(b * kUShortMax));
    color.ct.argb.pad   = 0;
    return color;
}

// qcolor.cpp:2514 fromHsv：h 必须 ∈ [-1,359]（**h>=360 返回无效**，与 setHsv 回绕不同）。
PkColor PkColor::fromHsv(int h, int s, int v, int a)
{
    if (((h < 0 || h >= 360) && h != -1)
        || s < 0 || s > 255
        || v < 0 || v > 255
        || a < 0 || a > 255) {
        return PkColor();
    }
    PkColor color;
    color.cspec = Hsv;
    color.ct.ahsv.alpha      = quint16(a * 0x101);
    color.ct.ahsv.hue        = h == -1 ? kUShortMax : quint16((h % 360) * 100);
    color.ct.ahsv.saturation = quint16(s * 0x101);
    color.ct.ahsv.value      = quint16(v * 0x101);
    color.ct.ahsv.pad        = 0;
    return color;
}

// qcolor.cpp:2545 fromHsvF。
PkColor PkColor::fromHsvF(qreal h, qreal s, qreal v, qreal a)
{
    if (((h < qreal(0.0) || h > qreal(1.0)) && h != qreal(-1.0))
        || (s < qreal(0.0) || s > qreal(1.0))
        || (v < qreal(0.0) || v > qreal(1.0))
        || (a < qreal(0.0) || a > qreal(1.0))) {
        return PkColor();
    }
    PkColor color;
    color.cspec = Hsv;
    color.ct.ahsv.alpha      = quint16(qRound(a * kUShortMax));
    color.ct.ahsv.hue        = h == qreal(-1.0) ? kUShortMax : quint16(qRound(h * 36000));
    color.ct.ahsv.saturation = quint16(qRound(s * kUShortMax));
    color.ct.ahsv.value      = quint16(qRound(v * kUShortMax));
    color.ct.ahsv.pad        = 0;
    return color;
}

// qcolor.cpp:2577 fromHsl。
PkColor PkColor::fromHsl(int h, int s, int l, int a)
{
    if (((h < 0 || h >= 360) && h != -1)
        || s < 0 || s > 255
        || l < 0 || l > 255
        || a < 0 || a > 255) {
        return PkColor();
    }
    PkColor color;
    color.cspec = Hsl;
    color.ct.ahsl.alpha      = quint16(a * 0x101);
    color.ct.ahsl.hue        = h == -1 ? kUShortMax : quint16((h % 360) * 100);
    color.ct.ahsl.saturation = quint16(s * 0x101);
    color.ct.ahsl.lightness  = quint16(l * 0x101);
    color.ct.ahsl.pad        = 0;
    return color;
}

// qcolor.cpp:2609 fromHslF：hue==36000 → 0（fromHslF 特有，setHslF 没有）。
PkColor PkColor::fromHslF(qreal h, qreal s, qreal l, qreal a)
{
    if (((h < qreal(0.0) || h > qreal(1.0)) && h != qreal(-1.0))
        || (s < qreal(0.0) || s > qreal(1.0))
        || (l < qreal(0.0) || l > qreal(1.0))
        || (a < qreal(0.0) || a > qreal(1.0))) {
        return PkColor();
    }
    PkColor color;
    color.cspec = Hsl;
    color.ct.ahsl.alpha      = quint16(qRound(a * kUShortMax));
    color.ct.ahsl.hue        = h == qreal(-1.0) ? kUShortMax : quint16(qRound(h * 36000));
    if (color.ct.ahsl.hue == 36000) color.ct.ahsl.hue = 0;
    color.ct.ahsl.saturation = quint16(qRound(s * kUShortMax));
    color.ct.ahsl.lightness  = quint16(qRound(l * kUShortMax));
    color.ct.ahsl.pad        = 0;
    return color;
}

PkColor PkColor::fromWireState(const WireState &state) noexcept
{
    PkColor color;
    if (state.spec < Invalid || state.spec > ExtendedRgb) return color;
    color.cspec = state.spec;
    if (state.spec == ExtendedRgb) {
        color.ct.argbExt.alphaF = halfToFloat(state.channels[0]);
        color.ct.argbExt.redF = halfToFloat(state.channels[1]);
        color.ct.argbExt.greenF = halfToFloat(state.channels[2]);
        color.ct.argbExt.blueF = halfToFloat(state.channels[3]);
        color.extendedWirePad = state.channels[4];
    } else {
        for (int i = 0; i < 5; ++i) color.ct.array[i] = state.channels[i];
    }
    return color;
}

PkColor::WireState PkColor::wireState() const noexcept
{
    WireState state{cspec, {0u, 0u, 0u, 0u, 0u}};
    if (cspec == ExtendedRgb) {
        state.channels[0] = floatToHalf(ct.argbExt.alphaF);
        state.channels[1] = floatToHalf(ct.argbExt.redF);
        state.channels[2] = floatToHalf(ct.argbExt.greenF);
        state.channels[3] = floatToHalf(ct.argbExt.blueF);
        state.channels[4] = extendedWirePad;
    } else {
        for (int i = 0; i < 5; ++i) state.channels[i] = ct.array[i];
    }
    return state;
}

// ---------------------------------------------------------------------------
// 分量 getter（8-bit 全部经 qt_div_257；非本 spec 的颜色先转换）
// ---------------------------------------------------------------------------

int PkColor::red() const noexcept
{
    if (cspec != Invalid && cspec != Rgb) return toRgb().red();
    return qt_div_257(ct.argb.red);
}
int PkColor::green() const noexcept
{
    if (cspec != Invalid && cspec != Rgb) return toRgb().green();
    return qt_div_257(ct.argb.green);
}
int PkColor::blue() const noexcept
{
    if (cspec != Invalid && cspec != Rgb) return toRgb().blue();
    return qt_div_257(ct.argb.blue);
}
int PkColor::alpha() const noexcept
{
    if (cspec == ExtendedRgb) return qRound((qreal)ct.argbExt.alphaF * 255);
    return qt_div_257(ct.argb.alpha);
}

// qcolor.cpp getHsv 语义（hsvHue）：hue==USHRT_MAX → -1，否则 hue/100。
int PkColor::hue() const noexcept
{
    if (cspec != Invalid && cspec != Hsv) return toHsv().hue();
    return ct.ahsv.hue == kUShortMax ? -1 : ct.ahsv.hue / 100;
}
int PkColor::saturation() const noexcept
{
    if (cspec != Invalid && cspec != Hsv) return toHsv().saturation();
    return qt_div_257(ct.ahsv.saturation);
}
int PkColor::value() const noexcept
{
    if (cspec != Invalid && cspec != Hsv) return toHsv().value();
    return qt_div_257(ct.ahsv.value);
}
int PkColor::hslHue() const noexcept
{
    if (cspec != Invalid && cspec != Hsl) return toHsl().hslHue();
    return ct.ahsl.hue == kUShortMax ? -1 : ct.ahsl.hue / 100;
}
int PkColor::hslSaturation() const noexcept
{
    if (cspec != Invalid && cspec != Hsl) return toHsl().hslSaturation();
    return qt_div_257(ct.ahsl.saturation);
}
int PkColor::lightness() const noexcept
{
    if (cspec != Invalid && cspec != Hsl) return toHsl().lightness();
    return qt_div_257(ct.ahsl.lightness);
}

// qcolor.cpp:1626 redF 等：Rgb/Invalid → 直接除；ExtendedRgb → 存的浮点；其余 → toRgb。
qreal PkColor::redF() const noexcept
{
    if (cspec == Rgb || cspec == Invalid) return ct.argb.red / qreal(kUShortMax);
    if (cspec == ExtendedRgb) return ct.argbExt.redF;
    return toRgb().redF();
}
qreal PkColor::greenF() const noexcept
{
    if (cspec == Rgb || cspec == Invalid) return ct.argb.green / qreal(kUShortMax);
    if (cspec == ExtendedRgb) return ct.argbExt.greenF;
    return toRgb().greenF();
}
qreal PkColor::blueF() const noexcept
{
    if (cspec == Rgb || cspec == Invalid) return ct.argb.blue / qreal(kUShortMax);
    if (cspec == ExtendedRgb) return ct.argbExt.blueF;
    return toRgb().blueF();
}
qreal PkColor::alphaF() const noexcept
{
    if (cspec == ExtendedRgb) return ct.argbExt.alphaF;
    return ct.argb.alpha / qreal(kUShortMax);
}

// qcolor.cpp:1391 rgba()：Invalid 也走 qt_div_257（alpha=65535 → 255）。
quint32 PkColor::rgba() const noexcept
{
    if (cspec != Invalid && cspec != Rgb) return toRgb().rgba();
    return pkRgba(qt_div_257(ct.argb.red), qt_div_257(ct.argb.green),
                  qt_div_257(ct.argb.blue), qt_div_257(ct.argb.alpha));
}
quint32 PkColor::rgb() const noexcept
{
    if (cspec != Invalid && cspec != Rgb) return toRgb().rgb();
    return pkRgb(qt_div_257(ct.argb.red), qt_div_257(ct.argb.green), qt_div_257(ct.argb.blue));
}

// ---------------------------------------------------------------------------
// 设定
// ---------------------------------------------------------------------------

void PkColor::setRed(int red) { intRangeCheck(red); if (cspec != Rgb) setRgb(red, green(), blue(), alpha()); else ct.argb.red = quint16(red * 0x101); }
void PkColor::setGreen(int green) { intRangeCheck(green); if (cspec != Rgb) setRgb(red(), green, blue(), alpha()); else ct.argb.green = quint16(green * 0x101); }
void PkColor::setBlue(int blue) { intRangeCheck(blue); if (cspec != Rgb) setRgb(red(), green(), blue, alpha()); else ct.argb.blue = quint16(blue * 0x101); }

void PkColor::setAlpha(int alpha)
{
    intRangeCheck(alpha);
    if (cspec == ExtendedRgb) { ct.argbExt.alphaF = (float)(alpha * (qreal(1.0) / 255)); return; }
    ct.argb.alpha = quint16(alpha * 0x101);
}

void PkColor::setAlphaF(qreal alpha)
{
    realRangeCheck(alpha);
    if (cspec == ExtendedRgb) { ct.argbExt.alphaF = (float)alpha; return; }
    ct.argb.alpha = quint16(qRound(alpha * kUShortMax));
}

// qcolor.cpp:1365 setRgb(int,int,int,int)：越界 → 置无效。
void PkColor::setRgb(int r, int g, int b, int a)
{
    if (!isRgbaValid(r, g, b, a)) { invalidate(); return; }
    cspec = Rgb;
    ct.argb.alpha = quint16(a * 0x101);
    ct.argb.red   = quint16(r * 0x101);
    ct.argb.green = quint16(g * 0x101);
    ct.argb.blue  = quint16(b * 0x101);
    ct.argb.pad   = 0;
}

void PkColor::setRgb(quint32 rgb) noexcept
{
    cspec = Rgb;
    ct.argb.alpha = 0xffff;
    ct.argb.red   = quint16(pkRed(rgb) * 0x101);
    ct.argb.green = quint16(pkGreen(rgb) * 0x101);
    ct.argb.blue  = quint16(pkBlue(rgb) * 0x101);
    ct.argb.pad   = 0;
}

void PkColor::setRgba(quint32 rgba) noexcept
{
    cspec = Rgb;
    ct.argb.alpha = quint16(pkAlpha(rgba) * 0x101);
    ct.argb.red   = quint16(pkRed(rgba) * 0x101);
    ct.argb.green = quint16(pkGreen(rgba) * 0x101);
    ct.argb.blue  = quint16(pkBlue(rgba) * 0x101);
    ct.argb.pad   = 0;
}

// qcolor.cpp:1332 setRgbF：alpha 越界 → 置无效；rgb 越界或已是 ExtendedRgb → 存浮点。
void PkColor::setRgbF(qreal r, qreal g, qreal b, qreal a)
{
    if (a < qreal(0.0) || a > qreal(1.0)) { invalidate(); return; }
    if (r < qreal(0.0) || r > qreal(1.0)
        || g < qreal(0.0) || g > qreal(1.0)
        || b < qreal(0.0) || b > qreal(1.0) || cspec == ExtendedRgb) {
        cspec = ExtendedRgb;
        ct.argbExt.redF   = (float)r;
        ct.argbExt.greenF = (float)g;
        ct.argbExt.blueF  = (float)b;
        ct.argbExt.alphaF = (float)a;
        return;
    }
    cspec = Rgb;
    ct.argb.red   = quint16(qRound(r * kUShortMax));
    ct.argb.green = quint16(qRound(g * kUShortMax));
    ct.argb.blue  = quint16(qRound(b * kUShortMax));
    ct.argb.alpha = quint16(qRound(a * kUShortMax));
    ct.argb.pad   = 0;
}

// qcolor.cpp:1112 setHsv：越界 → 置无效；h 超界回绕 h%360（h==-1 → USHRT_MAX）。
void PkColor::setHsv(int h, int s, int v, int a)
{
    if (h < -1 || (quint32)s > 255 || (quint32)v > 255 || (quint32)a > 255) { invalidate(); return; }
    cspec = Hsv;
    ct.ahsv.alpha      = quint16(a * 0x101);
    ct.ahsv.hue        = h == -1 ? kUShortMax : quint16((h % 360) * 100);
    ct.ahsv.saturation = quint16(s * 0x101);
    ct.ahsv.value      = quint16(v * 0x101);
    ct.ahsv.pad        = 0;
}

// qcolor.cpp:1085 setHsvF：越界 → **静默 return（不置无效）**，与 setHsv 不对称。
void PkColor::setHsvF(qreal h, qreal s, qreal v, qreal a)
{
    if (((h < qreal(0.0) || h > qreal(1.0)) && h != qreal(-1.0))
        || (s < qreal(0.0) || s > qreal(1.0))
        || (v < qreal(0.0) || v > qreal(1.0))
        || (a < qreal(0.0) || a > qreal(1.0))) {
        return;
    }
    cspec = Hsv;
    ct.ahsv.alpha      = quint16(qRound(a * kUShortMax));
    ct.ahsv.hue        = h == qreal(-1.0) ? kUShortMax : quint16(qRound(h * 36000));
    ct.ahsv.saturation = quint16(qRound(s * kUShortMax));
    ct.ahsv.value      = quint16(qRound(v * kUShortMax));
    ct.ahsv.pad        = 0;
}

// qcolor.cpp:1227 setHsl。
void PkColor::setHsl(int h, int s, int l, int a)
{
    if (h < -1 || (quint32)s > 255 || (quint32)l > 255 || (quint32)a > 255) { invalidate(); return; }
    cspec = Hsl;
    ct.ahsl.alpha      = quint16(a * 0x101);
    ct.ahsl.hue        = h == -1 ? kUShortMax : quint16((h % 360) * 100);
    ct.ahsl.saturation = quint16(s * 0x101);
    ct.ahsl.lightness  = quint16(l * 0x101);
    ct.ahsl.pad        = 0;
}

// qcolor.cpp:1198 setHslF（无 36000→0 修正，那是 fromHslF 特有）。
void PkColor::setHslF(qreal h, qreal s, qreal l, qreal a)
{
    if (((h < qreal(0.0) || h > qreal(1.0)) && h != qreal(-1.0))
        || (s < qreal(0.0) || s > qreal(1.0))
        || (l < qreal(0.0) || l > qreal(1.0))
        || (a < qreal(0.0) || a > qreal(1.0))) {
        return;
    }
    cspec = Hsl;
    ct.ahsl.alpha      = quint16(qRound(a * kUShortMax));
    ct.ahsl.hue        = h == qreal(-1.0) ? kUShortMax : quint16(qRound(h * 36000));
    ct.ahsl.saturation = quint16(qRound(s * kUShortMax));
    ct.ahsl.lightness  = quint16(qRound(l * kUShortMax));
    ct.ahsl.pad        = 0;
}

// ---------------------------------------------------------------------------
// 规范转换：toRgb / toHsv / toHsl / convertTo（darker/lighter 内部也要用）
// ---------------------------------------------------------------------------

// qcolor.cpp:2050 toRgb：逐字照抄（含 Hsl 的 array[i+1] 写入与 ==1→0 清理）。
PkColor PkColor::toRgb() const noexcept
{
    if (!isValid() || cspec == Rgb) return *this;

    PkColor color;
    color.cspec = Rgb;
    if (cspec != ExtendedRgb)
        color.ct.argb.alpha = ct.argb.alpha;
    color.ct.argb.pad = 0;

    switch (cspec) {
    case Hsv: {
        if (ct.ahsv.saturation == 0 || ct.ahsv.hue == kUShortMax) {
            color.ct.argb.red = color.ct.argb.green = color.ct.argb.blue = ct.ahsv.value;
            break;
        }
        const qreal h = ct.ahsv.hue == 36000 ? 0 : ct.ahsv.hue / 6000.;
        const qreal s = ct.ahsv.saturation / qreal(kUShortMax);
        const qreal v = ct.ahsv.value / qreal(kUShortMax);
        const int i = int(h);
        const qreal f = h - i;
        const qreal p = v * (qreal(1.0) - s);

        if (i & 1) {
            const qreal q = v * (qreal(1.0) - (s * f));
            switch (i) {
            case 1:
                color.ct.argb.red   = quint16(qRound(q * kUShortMax));
                color.ct.argb.green = quint16(qRound(v * kUShortMax));
                color.ct.argb.blue  = quint16(qRound(p * kUShortMax));
                break;
            case 3:
                color.ct.argb.red   = quint16(qRound(p * kUShortMax));
                color.ct.argb.green = quint16(qRound(q * kUShortMax));
                color.ct.argb.blue  = quint16(qRound(v * kUShortMax));
                break;
            case 5:
                color.ct.argb.red   = quint16(qRound(v * kUShortMax));
                color.ct.argb.green = quint16(qRound(p * kUShortMax));
                color.ct.argb.blue  = quint16(qRound(q * kUShortMax));
                break;
            }
        } else {
            const qreal t = v * (qreal(1.0) - (s * (qreal(1.0) - f)));
            switch (i) {
            case 0:
                color.ct.argb.red   = quint16(qRound(v * kUShortMax));
                color.ct.argb.green = quint16(qRound(t * kUShortMax));
                color.ct.argb.blue  = quint16(qRound(p * kUShortMax));
                break;
            case 2:
                color.ct.argb.red   = quint16(qRound(p * kUShortMax));
                color.ct.argb.green = quint16(qRound(v * kUShortMax));
                color.ct.argb.blue  = quint16(qRound(t * kUShortMax));
                break;
            case 4:
                color.ct.argb.red   = quint16(qRound(t * kUShortMax));
                color.ct.argb.green = quint16(qRound(p * kUShortMax));
                color.ct.argb.blue  = quint16(qRound(v * kUShortMax));
                break;
            }
        }
        break;
    }
    case Hsl: {
        if (ct.ahsl.saturation == 0 || ct.ahsl.hue == kUShortMax) {
            color.ct.argb.red = color.ct.argb.green = color.ct.argb.blue = ct.ahsl.lightness;
        } else if (ct.ahsl.lightness == 0) {
            color.ct.argb.red = color.ct.argb.green = color.ct.argb.blue = 0;
        } else {
            const qreal h = ct.ahsl.hue == 36000 ? 0 : ct.ahsl.hue / 36000.;
            const qreal s = ct.ahsl.saturation / qreal(kUShortMax);
            const qreal l = ct.ahsl.lightness / qreal(kUShortMax);

            qreal temp2;
            if (l < qreal(0.5))
                temp2 = l * (qreal(1.0) + s);
            else
                temp2 = l + s - (l * s);

            const qreal temp1 = (qreal(2.0) * l) - temp2;
            qreal temp3[3] = { h + (qreal(1.0) / qreal(3.0)),
                               h,
                               h - (qreal(1.0) / qreal(3.0)) };

            for (int i = 0; i != 3; ++i) {
                if (temp3[i] < qreal(0.0))
                    temp3[i] += qreal(1.0);
                else if (temp3[i] > qreal(1.0))
                    temp3[i] -= qreal(1.0);

                const qreal sixtemp3 = temp3[i] * qreal(6.0);
                if (sixtemp3 < qreal(1.0))
                    color.ct.array[i + 1] = quint16(qRound((temp1 + (temp2 - temp1) * sixtemp3) * kUShortMax));
                else if ((temp3[i] * qreal(2.0)) < qreal(1.0))
                    color.ct.array[i + 1] = quint16(qRound(temp2 * kUShortMax));
                else if ((temp3[i] * qreal(3.0)) < qreal(2.0))
                    color.ct.array[i + 1] = quint16(qRound((temp1 + (temp2 - temp1) * (qreal(2.0) / qreal(3.0) - temp3[i]) * qreal(6.0)) * kUShortMax));
                else
                    color.ct.array[i + 1] = quint16(qRound(temp1 * kUShortMax));
            }
            color.ct.argb.red   = color.ct.argb.red   == 1 ? 0 : color.ct.argb.red;
            color.ct.argb.green = color.ct.argb.green == 1 ? 0 : color.ct.argb.green;
            color.ct.argb.blue  = color.ct.argb.blue  == 1 ? 0 : color.ct.argb.blue;
        }
        break;
    }
    case Cmyk: {
        // CMYK 系在范围表里是「不实现」（0 调用点，无 fromCmyk/setCmyk 入口），
        // 这里照抄 qcolor.cpp:2161 的转换，仅保证万一出现 Cmyk 色也能正确转回。
        const qreal c = ct.acmyk.cyan / qreal(kUShortMax);
        const qreal m = ct.acmyk.magenta / qreal(kUShortMax);
        const qreal y = ct.acmyk.yellow / qreal(kUShortMax);
        const qreal k = ct.acmyk.black / qreal(kUShortMax);
        color.ct.argb.red   = quint16(qRound((qreal(1.0) - (c * (qreal(1.0) - k) + k)) * kUShortMax));
        color.ct.argb.green = quint16(qRound((qreal(1.0) - (m * (qreal(1.0) - k) + k)) * kUShortMax));
        color.ct.argb.blue  = quint16(qRound((qreal(1.0) - (y * (qreal(1.0) - k) + k)) * kUShortMax));
        break;
    }
    case ExtendedRgb:
        color.ct.argb.alpha = quint16(qRound(kUShortMax * qreal(ct.argbExt.alphaF)));
        color.ct.argb.red   = quint16(qRound(kUShortMax * qBound(qreal(0.0), qreal(ct.argbExt.redF),   qreal(1.0))));
        color.ct.argb.green = quint16(qRound(kUShortMax * qBound(qreal(0.0), qreal(ct.argbExt.greenF), qreal(1.0))));
        color.ct.argb.blue  = quint16(qRound(kUShortMax * qBound(qreal(0.0), qreal(ct.argbExt.blueF),  qreal(1.0))));
        break;
    default:
        break;
    }

    return color;
}

// qcolor.cpp:2203 toHsv。
PkColor PkColor::toHsv() const noexcept
{
    if (!isValid() || cspec == Hsv) return *this;
    if (cspec != Rgb) return toRgb().toHsv();

    PkColor color;
    color.cspec = Hsv;
    color.ct.ahsv.alpha = ct.argb.alpha;
    color.ct.ahsv.pad = 0;

    const qreal r = ct.argb.red   / qreal(kUShortMax);
    const qreal g = ct.argb.green / qreal(kUShortMax);
    const qreal b = ct.argb.blue  / qreal(kUShortMax);
    const qreal max = qMax(r, qMax(g, b));
    const qreal min = qMin(r, qMin(g, b));
    const qreal delta = max - min;
    color.ct.ahsv.value = quint16(qRound(max * kUShortMax));
    if (pkQtFuzzyIsNull(delta)) {
        color.ct.ahsv.hue = kUShortMax;
        color.ct.ahsv.saturation = 0;
    } else {
        qreal hue = 0;
        color.ct.ahsv.saturation = quint16(qRound((delta / max) * kUShortMax));
        if (pkQtFuzzyCompare(r, max)) {
            hue = ((g - b) / delta);
        } else if (pkQtFuzzyCompare(g, max)) {
            hue = (qreal(2.0) + (b - r) / delta);
        } else if (pkQtFuzzyCompare(b, max)) {
            hue = (qreal(4.0) + (r - g) / delta);
        } else {
            Q_ASSERT(false && "PkColor::toHsv internal error");
        }
        hue *= qreal(60.0);
        if (hue < qreal(0.0))
            hue += qreal(360.0);
        color.ct.ahsv.hue = quint16(qRound(hue * 100));
    }

    return color;
}

// qcolor.cpp:2254 toHsl。
PkColor PkColor::toHsl() const noexcept
{
    if (!isValid() || cspec == Hsl) return *this;
    if (cspec != Rgb) return toRgb().toHsl();

    PkColor color;
    color.cspec = Hsl;
    color.ct.ahsl.alpha = ct.argb.alpha;
    color.ct.ahsl.pad = 0;

    const qreal r = ct.argb.red   / qreal(kUShortMax);
    const qreal g = ct.argb.green / qreal(kUShortMax);
    const qreal b = ct.argb.blue  / qreal(kUShortMax);
    const qreal max = qMax(r, qMax(g, b));
    const qreal min = qMin(r, qMin(g, b));
    const qreal delta = max - min;
    const qreal delta2 = max + min;
    const qreal lightness = qreal(0.5) * delta2;
    color.ct.ahsl.lightness = quint16(qRound(lightness * kUShortMax));
    if (pkQtFuzzyIsNull(delta)) {
        color.ct.ahsl.hue = kUShortMax;
        color.ct.ahsl.saturation = 0;
    } else {
        qreal hue = 0;
        if (lightness < qreal(0.5))
            color.ct.ahsl.saturation = quint16(qRound((delta / delta2) * kUShortMax));
        else
            color.ct.ahsl.saturation = quint16(qRound((delta / (qreal(2.0) - delta2)) * kUShortMax));
        if (pkQtFuzzyCompare(r, max)) {
            hue = ((g - b) / delta);
        } else if (pkQtFuzzyCompare(g, max)) {
            hue = (qreal(2.0) + (b - r) / delta);
        } else if (pkQtFuzzyCompare(b, max)) {
            hue = (qreal(4.0) + (r - g) / delta);
        } else {
            Q_ASSERT(false && "PkColor::toHsl internal error");
        }
        hue *= qreal(60.0);
        if (hue < qreal(0.0))
            hue += qreal(360.0);
        color.ct.ahsl.hue = quint16(qRound(hue * 100));
    }

    return color;
}

// qcolor.cpp:2351 convertTo。
PkColor PkColor::convertTo(PkColor::Spec colorSpec) const noexcept
{
    if (colorSpec == cspec) return *this;
    switch (colorSpec) {
    case Rgb:
        return toRgb();
    case Hsv:
        return toHsv();
    case Cmyk:
        // CMYK 系范围表判「不实现」；这里没有 toCmyk。落到 Cmyk 的 convertTo
        // 不可能被 darker/lighter 触发（它们只转回原 cspec，而原 cspec 不可能
        // 是 Cmyk——没有 setCmyk/fromCmyk 入口）。保持与「必须返回无效色」一致。
        return PkColor();
    case Hsl:
        return toHsl();
    case ExtendedRgb:
        // 没有 toExtendedRgb（范围表不实现）。darker/lighter 不会走到这里。
        return PkColor();
    case Invalid:
        break;
    }
    return PkColor();
}

// ---------------------------------------------------------------------------
// darker / lighter（qcolor.cpp:2845/2890）
// ---------------------------------------------------------------------------

PkColor PkColor::lighter(int factor) const noexcept
{
    if (factor <= 0) return *this;
    else if (factor < 100) return darker(10000 / factor);

    PkColor hsv = toHsv();
    int s = hsv.ct.ahsv.saturation;
    uint v = hsv.ct.ahsv.value;

    v = (factor * v) / 100;
    if (v > kUShortMax) {
        s -= int(v - kUShortMax);
        if (s < 0) s = 0;
        v = kUShortMax;
    }

    hsv.ct.ahsv.saturation = quint16(s);
    hsv.ct.ahsv.value = quint16(v);

    return hsv.convertTo(cspec);
}

PkColor PkColor::darker(int factor) const noexcept
{
    if (factor <= 0) return *this;
    else if (factor < 100) return lighter(10000 / factor);

    PkColor hsv = toHsv();
    hsv.ct.ahsv.value = quint16((hsv.ct.ahsv.value * 100) / factor);

    return hsv.convertTo(cspec);
}

// ---------------------------------------------------------------------------
// name() / name(Format)（qcolor.cpp:867/880）
// ---------------------------------------------------------------------------

PkString PkColor::name() const { return name(HexRgb); }

PkString PkColor::name(NameFormat format) const
{
    char buf[16];
    switch (format) {
    case HexRgb:
        std::snprintf(buf, sizeof buf, "#%06x", unsigned(rgba() & 0xffffff));
        break;
    case HexArgb:
        std::snprintf(buf, sizeof buf, "#%08x", unsigned(rgba()));
        break;
    default:
        return PkString();
    }
    return PkString(buf);
}

// ---------------------------------------------------------------------------
// setNamedColor（qcolor.cpp:917 走 setColorFromString 的模板；本类只有 UTF-8 形态）
// ---------------------------------------------------------------------------

void PkColor::setNamedColor(const char *name)
{
    if (!name || !*name) { invalidate(); return; }

    if (name[0] == '#') {
        quint16 a, r, g, b;
        if (getHexRgb(name, std::strlen(name), &a, &r, &g, &b)) {
            setRgba64(a, r, g, b);
            return;
        }
        invalidate();
        return;
    }

    quint32 rgb;
    if (getNamedRgb(name, int(std::strlen(name)), &rgb)) {
        setRgba(rgb);
        return;
    }
    invalidate();
}

void PkColor::setNamedColor(const PkString &name)
{
    const std::string utf8 = name.PkToUtf8();
    setNamedColor(utf8.c_str());
}

// ---------------------------------------------------------------------------
// operator==（qcolor.cpp:2954：**比较 alpha**，且 Hsl 分支有容差、ExtendedRgb
// 分支走浮点模糊比较）
// ---------------------------------------------------------------------------

bool PkColor::operator==(const PkColor &color) const noexcept
{
    if (cspec == Hsl && cspec == color.cspec) {
        return (ct.argb.alpha == color.ct.argb.alpha
                && ct.ahsl.hue % 36000 == color.ct.ahsl.hue % 36000
                && (qAbs(ct.ahsl.saturation - color.ct.ahsl.saturation) < 50
                    || ct.ahsl.lightness == 0
                    || color.ct.ahsl.lightness == 0
                    || ct.ahsl.lightness == kUShortMax
                    || color.ct.ahsl.lightness == kUShortMax)
                && (qAbs(ct.ahsl.lightness - color.ct.ahsl.lightness)) < 50);
    } else if ((cspec == ExtendedRgb || color.cspec == ExtendedRgb) &&
               (cspec == color.cspec || cspec == Rgb || color.cspec == Rgb)) {
        return pkQtFuzzyCompare(alphaF(), color.alphaF())
            && pkQtFuzzyCompare(redF(), color.redF())
            && pkQtFuzzyCompare(greenF(), color.greenF())
            && pkQtFuzzyCompare(blueF(), color.blueF());
    } else {
        return (cspec == color.cspec
                && ct.argb.alpha == color.ct.argb.alpha
                && (((cspec == PkColor::Hsv)
                     && ((ct.ahsv.hue % 36000) == (color.ct.ahsv.hue % 36000)))
                    || (ct.ahsv.hue == color.ct.ahsv.hue))
                && ct.argb.green == color.ct.argb.green
                && ct.argb.blue  == color.ct.argb.blue
                && ct.argb.pad   == color.ct.argb.pad);
    }
}

// ---------------------------------------------------------------------------
// 私有工具
// ---------------------------------------------------------------------------

// qcolor.cpp:3008 invalidate：alpha 置 65535（兼容 Qt 3 的「无效色 alpha 仍全不透」）。
void PkColor::invalidate() noexcept
{
    cspec = Invalid;
    ct.argb.alpha = kUShortMax;
    ct.argb.red = 0; ct.argb.green = 0; ct.argb.blue = 0; ct.argb.pad = 0;
}

// qcolor.cpp:1437 setRgba64（内部形态：4 个 quint16）。
void PkColor::setRgba64(quint16 a, quint16 r, quint16 g, quint16 b) noexcept
{
    cspec = Rgb;
    ct.argb.alpha = a;
    ct.argb.red = r;
    ct.argb.green = g;
    ct.argb.blue = b;
    ct.argb.pad = 0;
}
