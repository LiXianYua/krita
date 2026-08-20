#ifndef PK_COLOR_PKCOLOR_H
#define PK_COLOR_PKCOLOR_H

// ---------------------------------------------------------------------------
// PkColor —— 对齐 Qt 5.15.7 QColor 的零 Qt 依赖颜色类。
//
// 对齐口径与 R-03/R-13/R-18 相同：与 Qt 的任何行为差异默认都是缺陷，Qt 那些
// 看着像 bug 的地方也照抄（setHsl(0,255,128) 会给出 (255,1,1) 而非 (255,0,0)、
// setHsvF 参数越界是静默 return 而 setHsv 是置无效、operator== 会比较 alpha）。
// 实现是逐字照抄真 Qt 5.15.7 的 qcolor.cpp / qcolor.h（探针 + 源码对照），
// 内部存储结构与 Qt 的 CT union 同构（16-bit 每通道 + Spec），这样 toHsv /
// toHsl / toRgb / darker / lighter 的浮点取整路径（qreal/65535 + qRound +
// qt_div_257）能与真 Qt 逐位一致，对拍 oracle 才能 mismatch=0。
//
// 交付面 = brief 范围表 + Step 2 的 API 形状，再按真 Qt 5.15 订正过的版本：
//   · setRgba(quint32) 是**单参 QRgb**（Qt 5.15 没有 4 参 setRgba）；
//   · 没有 setRgbaF（Qt 5.15 没有）；
//   · Spec 枚举含 ExtendedRgb=5（setRgbF 越界会落到它，见 PkColor.cpp）；
//   · fromString 不存在（Qt 5.15 没有），解析入口是 setNamedColor / 构造。
// 每条偏离在 README「偏离登记」里逐条声明。
// ---------------------------------------------------------------------------

#include "../global/PkGlobal.h"     // qreal / quint16 / qAbs / qRound / qMin ...
#include "../namespace/PkNamespace.h"   // Qt::GlobalColor（R-27 Task 2 交付）
#include "../string/PkString.h"     // name() 返回类型

class PkColor
{
public:
    // 对齐真 Qt 5.15 qcolor.h：Invalid=0 Rgb=1 Hsv=2 Cmyk=3 Hsl=4 ExtendedRgb=5。
    enum Spec {
        Invalid = 0,
        Rgb = 1,
        Hsv = 2,
        Cmyk = 3,
        Hsl = 4,
        ExtendedRgb = 5
    };

    enum NameFormat {
        HexRgb,
        HexArgb
    };

    // QColor's Qt_4_6 QDataStream payload is its Spec plus five 16-bit words.
    // This public value type exposes that persistence state without revealing
    // the private union layout or reducing HSV/HSL/CMYK/ExtendedRgb to RGB.
    struct WireState {
        Spec spec;
        quint16 channels[5];

        bool operator==(const WireState &other) const noexcept
        {
            if (spec != other.spec) return false;
            for (int i = 0; i < 5; ++i) {
                if (channels[i] != other.channels[i]) return false;
            }
            return true;
        }
        bool operator!=(const WireState &other) const noexcept { return !(*this == other); }
    };

    // ── 构造 ───────────────────────────────────────────────
    PkColor() noexcept;                                  // 无效，alpha=65535（rgba() 的 alpha 仍 255）
    PkColor(int r, int g, int b, int a = 255) noexcept;  // 越界 → 无效（分量全 0，含 alpha）
    PkColor(Qt::GlobalColor color) noexcept;             // GlobalColor 20 项表，含 transparent
    PkColor(const char *name);                           // setNamedColor 语义（SVG 命名色 / #hex）
    PkColor(const PkString &name);                       // 同上，PkString 形态（Krita QString→PkString）
    PkColor(const PkColor &other) noexcept = default;
    ~PkColor() = default;

    // ── 赋值 ───────────────────────────────────────────────
    PkColor &operator=(const PkColor &other) noexcept = default;
    PkColor &operator=(Qt::GlobalColor color) noexcept;

    // ── 静态工厂 ───────────────────────────────────────────
    static PkColor fromRgb(int r, int g, int b, int a = 255);
    static PkColor fromRgb(quint32 rgb) noexcept;        // opaque，alpha 置 255
    static PkColor fromRgba(quint32 rgba) noexcept;
    static PkColor fromRgbF(qreal r, qreal g, qreal b, qreal a = 1.0);
    static PkColor fromHsv(int h, int s, int v, int a = 255);
    static PkColor fromHsvF(qreal h, qreal s, qreal v, qreal a = 1.0);
    static PkColor fromHsl(int h, int s, int l, int a = 255);
    static PkColor fromHslF(qreal h, qreal s, qreal l, qreal a = 1.0);
    static PkColor fromWireState(const WireState &state) noexcept;

    // ── 状态 ───────────────────────────────────────────────
    bool isValid() const noexcept { return cspec != Invalid; }
    Spec spec() const noexcept { return cspec; }
    WireState wireState() const noexcept;

    // ── 8-bit 分量 getter ──────────────────────────────────
    int red() const noexcept;
    int green() const noexcept;
    int blue() const noexcept;
    int alpha() const noexcept;
    int hue() const noexcept;            // HSV hue（灰 = -1）
    int saturation() const noexcept;     // HSV saturation
    int value() const noexcept;          // HSV value
    int hslHue() const noexcept;         // HSL hue（灰 = -1）
    int hslSaturation() const noexcept;  // HSL saturation
    int lightness() const noexcept;      // HSL lightness

    // ── 浮点分量 getter ────────────────────────────────────
    qreal redF() const noexcept;
    qreal greenF() const noexcept;
    qreal blueF() const noexcept;
    qreal alphaF() const noexcept;

    // ── 合成取色 ───────────────────────────────────────────
    quint32 rgba() const noexcept;       // 0xaarrggbb（无效色 = 0xff000000）
    quint32 rgb() const noexcept;        // 0x00rrggbb（alpha 恒 255）
    quint32 toArgb32() const noexcept { return rgba(); }

    // ── 设定 ───────────────────────────────────────────────
    void setRed(int red);                // 越界 → 截断（不置无效）；非 Rgb 色先转 Rgb
    void setGreen(int green);
    void setBlue(int blue);
    void setAlpha(int alpha);
    void setAlphaF(qreal alpha);
    void setRgb(int r, int g, int b, int a = 255);   // 越界 → 置无效
    void setRgb(quint32 rgb) noexcept;   // opaque，alpha 置 255
    void setRgba(quint32 rgba) noexcept; // 单参 QRgb（Qt 5.15 签名）
    void setRgbF(qreal r, qreal g, qreal b, qreal a = 1.0);  // rgb 越界 → ExtendedRgb
    void setHsv(int h, int s, int v, int a = 255);   // 越界 → 置无效；h 超界回绕 h%360
    void setHsvF(qreal h, qreal s, qreal v, qreal a = 1.0); // 越界 → 静默 return（不置无效）
    void setHsl(int h, int s, int l, int a = 255);
    void setHslF(qreal h, qreal s, qreal l, qreal a = 1.0);
    void setNamedColor(const char *name);            // SVG 命名色 / #RGB / #RRGGBB / #AARRGGBB ...
    void setNamedColor(const PkString &name);

    // ── 规范转换（真 Qt 公开 API；lighter/darker 与各 getter 内部也用）──
    PkColor toRgb() const noexcept;              // → Rgb spec（Hsv/Hsl/Cmyk/ExtendedRgb 转回）
    PkColor toHsv() const noexcept;              // → Hsv spec
    PkColor toHsl() const noexcept;              // → Hsl spec
    PkColor convertTo(Spec colorSpec) const noexcept;  // 与 Qt 同：无 toCmyk/toExtendedRgb 时返回无效色

    // ── 派生 ───────────────────────────────────────────────
    PkColor lighter(int factor = 150) const noexcept;   // factor<=0 不变；<100 交叉调 darker(10000/f)
    PkColor darker(int factor = 200) const noexcept;    // factor<=0 不变；<100 交叉调 lighter(10000/f)

    // ── 命名 ───────────────────────────────────────────────
    PkString name() const;                       // HexRgb：#rrggbb（不带 alpha）
    PkString name(NameFormat format) const;      // HexArgb：#aarrggbb

    // ── 比较（对齐 Qt 5.15：**比较 alpha**，且要求 cspec 相同）──
    bool operator==(const PkColor &color) const noexcept;
    bool operator!=(const PkColor &color) const noexcept { return !operator==(color); }

private:
    void invalidate() noexcept;
    void setRgba64(quint16 a, quint16 r, quint16 g, quint16 b) noexcept;

    // 内部 16-bit 每通道存储，与 Qt 5.15 的 CT union 同构（含 ExtendedRgb 的
    // 浮点通道）。非 ExtendedRgb 时 union 前 12 字节 = 5 个 quint16（alpha 在
    // array[0]）。rgba()/red() 等 8-bit getter 一律经 qt_div_257 换算。
    union {
        struct { quint16 alpha, red, green, blue, pad; } argb;
        struct { quint16 alpha, hue, saturation, value, pad; } ahsv;
        struct { quint16 alpha, hue, saturation, lightness, pad; } ahsl;
        struct { quint16 alpha, cyan, magenta, yellow, black, pad; } acmyk;
        struct { float alphaF, redF, greenF, blueF; } argbExt;
        quint16 array[6];
    } ct;

    Spec cspec;
    quint16 extendedWirePad = 0;
};

#endif // PK_COLOR_PKCOLOR_H
