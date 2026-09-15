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
#include "../namespace/PkNamespace.h"   // Pk::GlobalColor（R-27 Task 2 交付）
#include "../string/PkString.h"     // name() 返回类型

// R-87：下面那条 std::ostream 插入运算符的声明只需要 <iosfwd> 的前向声明。
// ⚠ **这是本头唯一一条系统头**。把本头 include 进某个 namespace 的 TU（
//   oracle/difftest_color.cpp 的 `namespace pkoracle`）必须把 <iosfwd> **先在
//   namespace 之外** include 一次，否则会造出 pkoracle::std 遮住 ::std
//   （该文件顶部那条「std 系统头必须在包外层先 include」的注释就是这条纪律；
//   与 pk/geometry 对 PkSize.cpp <type_traits> 的处理同型，见
//   pk/geometry/oracle/geometry_difftest.cpp:119-120）。
#include <iosfwd>

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
    PkColor(Pk::GlobalColor color) noexcept;             // GlobalColor 20 项表，含 transparent
    PkColor(const char *name);                           // setNamedColor 语义（SVG 命名色 / #hex）
    PkColor(const PkString &name);                       // 同上，PkString 形态（Krita QString→PkString）
    PkColor(const PkColor &other) noexcept = default;
    ~PkColor() = default;

    // ── 赋值 ───────────────────────────────────────────────
    PkColor &operator=(const PkColor &other) noexcept = default;
    PkColor &operator=(Pk::GlobalColor color) noexcept;

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

    // ── 组合 getter（输出指针语义对齐 QColor 5.15）────────
    void getHsv(int *h, int *s, int *v, int *a = nullptr) const noexcept;
    void getHsvF(qreal *h, qreal *s, qreal *v, qreal *a = nullptr) const noexcept;
    void getHslF(qreal *h, qreal *s, qreal *l, qreal *a = nullptr) const noexcept;

    // ── 合成取色 ───────────────────────────────────────────
    quint32 rgba() const noexcept;       // 0xaarrggbb（无效色 = 0xff000000）
    quint32 rgb() const noexcept;        // 0x00rrggbb（alpha 恒 255）
    quint32 toArgb32() const noexcept { return rgba(); }

    // ── 设定 ───────────────────────────────────────────────
    void setRed(int red);                // 越界 → 截断（不置无效）；非 Rgb 色先转 Rgb
    void setGreen(int green);
    void setBlue(int blue);
    void setAlpha(int alpha);
    qreal lightnessF() const noexcept;                 // 对齐 QColor::lightnessF（S-09-g kis_painting_tweaks 12 处）
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
    // 解析成功与否（KoSvgTextProperties 的颜色 token 校验，S-09-g）。
    static bool isValidColor(const PkString &name);

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

// R-87：PK_COMPARE 诊断通道用的 `std::ostream` 插入运算符。
//
// **文本 = 真 Qt 5.15.7 `QDebug operator<<(QDebug, const QColor&)` 的原文**
// （qcolor.cpp「QColor stream functions」一节；声明在 QtGui/qcolor.h:56-57，
//  `#ifndef QT_NO_DEBUG_STREAM`），只把类型名换成 PkColor。分支与文本逐条照抄：
//   Invalid    → `PkColor(Invalid)`
//   Rgb        → `PkColor(ARGB <aF>, <rF>, <gF>, <bF>)`
//   ExtendedRgb→ `PkColor(Ext. ARGB …)`
//   Hsv        → `PkColor(AHSV …)`（取值经 getHsvF —— PkColor 没有 hueF/saturationF/
//                valueF 三个单分量 F 取值器；Qt 那条分支用的就是它们）
//   Hsl        → `PkColor(AHSL …)`（同上，经 getHslF）
//   Cmyk       → 见 PkColorTestString.cpp 的注释：PkColor 没有 cyanF/magentaF/
//                yellowF/blackF（实测用量 0，未交付），复刻不出 Qt 的 `ACMYK` 文本。
//
// ⚠ **这是一条超出 Qt 的扩面（Q2-a 裁定），不是对齐 Qt**：真 Qt 的
//   `QCOMPARE(QColor, QColor)` 判红时**也**打 `<unprintable>`（`QTest::toString<QColor>`
//   返回 `nullptr`，探针① 实测）—— Qt 里这条文本只出现在 QDebug 通道、
//   不在 QTest 通道。**所以「Qt 里没有对应物」，这段文本是本仓的选择。**
//   选它的理由是它是 Qt 对 QColor **唯一存在的**文本；登记见 README 偏离 9。
//   代价实测过两次：R-75 把 `<unprintable>` 误判成「剥 Qt 后的行为差异」、
//   R-78 因它白立一次案。
//
// ⚠ **连带变更（R-87 修复轮登记）**：本运算符让 `PkDebugIsOstreamable<PkColor>`
//   由 0 变 1（pk/log/PkDebug.h:43-49 的 SFINAE 探针），于是 `qDebug() << pkColor`
//   从「分支三」的 `<unprintable>` 改走「分支二」打本运算符的值文本。**方向上这是
//   朝真 Qt 收敛、不是偏离** —— 真 Qt 的 `QDebug operator<<(QDebug, const QColor&)`
//   本来就打值（探针文本 `QColor(ARGB 1, 1, 0, 0)`，见 PkColorTestString.cpp 文件头）。
//   当前**不可观测**：全树零处同时「内联编 `PkColor.cpp`」+「用 `PkDebug` 流 `PkColor`」
//   （实测 `grep -rln PkColor.h pk/` 再筛 `PkDebug.h` 只命中两份 CMakeLists、无 TU）——
//   是「现在测不到」，不是「无影响」。登记见 README 偏离 10。
//   ⚠ **对照组 `PkRect` 没有这条连带**：它有自由
//   `PkDebug operator<<(PkDebug, const PkRect&)`（PkGeometryDebug.cpp:107），重载决议
//   仍选中那条，`dbg << rect` 文本一字未漂（`test_pkgeometry_debugstream` 全绿）——
//   **两边不同形**，别照搬。
//
// 定义在 pk/color/PkColorTestString.cpp（独立 TU，与任何 PkDebug 通道零耦合）。
std::ostream &operator<<(std::ostream &os, const PkColor &c);

#endif // PK_COLOR_PKCOLOR_H
