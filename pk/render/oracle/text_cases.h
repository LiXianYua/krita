// R-55 对拍工装 · 共用部分：同一份输入集喂真 Qt 与 Pk，两侧各编一份。
//
// 形态照 pk/render/oracle/shape_primitive_cases.h（R-51 交付）与
// blur_kernel_cases.h（R-52 交付）：本头被两个 TU 各编一次，双分支靠
// PK_TEXT_QT_ORACLE 切换，**不是同一个实现换个壳**。
//
//   · oracle/text_oracle.cpp   加 -DPK_TEXT_QT_ORACLE → 真 QPainter
//                              （只由 run_text.sh 手工编译，不进任何产物）
//   · tests/test_text.cpp      （不加宏）            → PkPainter + PkImageRasterBackend
//
// 复刻对象 = Krita v6.0.3 两处真实 drawText 调用点（R-55 plan §1.1 逐处核实）：
//   1) libs/flake/text/KoSvgTextShape_p_output.cpp:686（Private::paintDebug）
//        painter.save(); painter.setTransform(PkTransform());      // 显式重置成单位阵
//        painter.setPen(PkPen(Pk::red));
//        painter.drawText(viewCenter, text);                       // 默认字体、红笔
//        painter.restore();
//      text = "#" + number(i) [+ "~" + number(end)] + "\n(%1)".arg(idx) —— ASCII + 一个 \n。
//   2) plugins/tools/svgtexttool/SvgTextCursor.cpp:880（paintDecorations）
//        gc.setTransform(d->shape->absoluteTransformation(), true);   // 含缩放的仿射
//        gc.setPen(pen /* bgColorForCaret(selectionColor,255), 宽 decorationThickness */);
//        gc.setBrush(PkBrush(selectionColor));
//        gc.drawText(painterTf.map(closestBaselinePoint), name);      // name = i18nc 可翻译串
//      name 是可翻译手柄名（可含非 ASCII），位置经 painterTf 映射到设备空间。
//
// 两处都**不**调用 setFont（plan §1.1：`PkSetFontCommand` 直调 0 处）——默认字体即两种
// 调用点的实际形态。§6 第 3 条已登记「空 PkFont 的字体解析平台相关」。为覆盖
// `PkSetFontCommand` 本身，本表另加若干显式 setFont 用例（标注 api=setFont+drawText）。
//
// 本工装第一版发现的真实分歧（点重载下 `\n` 被 Qt 丢弃、Pk 却推进笔位）**已修**：
// `PkImageRasterBackend::drawText` 现在按 Qt 的 applyVisibilityRules 规则（出处见该文件
// 的 qtVisibleText() 注释）在送进 coverage() 前滤掉该集合。判据、扫描表与两侧实测数字
// 见 `.superpowers/sdd/R-55/task-4fix-report.md`。内建见证：`adv/newline-mid`（"X\nY"）
// 与 `adv/newline-twin`（"XY"）两条摘要**已相等**（修复前不等），`vis/hidden/lf` 与
// `vis/hidden/lf-twin` 同理。
//
// ⚠ 仍是已知分歧（**本批不修，见报告 §5**）：
//   · `vis/kept/tab`：Qt 的点重载走文本引擎的 tab stop（qtextengine.cpp:1341
//     calculateTabWidth，本机实测 px24 DejaVu 下 "X\tY" 推进 94.656、单字符 80.000），
//     Pk 把 U+0009 交给 Raqm（实测 7.000）。同层不同实现，**待裁决**。
//   · `vis/kept/del`（U+007F）与 `vis/kept/mvs`（U+180E）：Qt 本机走 CoreText 而 Pk 走
//     FreeType，属已登记的〈文字类对拍〉平台差异（plan §6 第 1 条），不是文本引擎规则。
//   · `vis/bidi/lri`（U+2066）与 `vis/bidi/alm`（U+061C）：Qt 侧是彻底的无操作，Pk 侧
//     Raqm 的 bidi 解析会挪动邻字（同 TAB，是 Raqm/Qt-bidi 的实现差异）。**未裁决**。
//   · `PkDrawTextInRectCommand`（rect+flags 重载）**零活调用点**且命令无 flags 字段，
//     从 Pk 侧根本表达不出来，本表不覆盖（plan §1.3 / §6 第 2 条）。
// `vis/*` 两组用例来自本轮的**系统性扫描**（不是 11 个样本的探测）：对 U+0000..U+001F、
// U+007F..U+009F、U+00A0、U+00AD、U+061C、U+180E、U+2000..U+200F、U+2028..U+202F、
// U+205F..U+2064、U+2066..U+206F、U+FEFF、U+FFF9..FFFB、U+110BD 逐个测两侧的
// 「推进量 + 墨迹」；完整表见 task-4fix-report.md §2。分组口径：
//   · `vis/hidden/*` = Qt 侧推进 0 且无墨（应当被丢掉的类别：C0 控制符、Separator、
//     bidi/format 控制符）。Pk 侧原本只剩 {LF, FF, CR} 还在推进 —— 这正是本轮修的。
//   · `vis/kept/*`   = Qt 侧推进非 0（保留的类别：TAB、VT、space 系、U+007F、U+180E）。
//     这些是**反向守卫**：谁把滤除集合开大（比如连 isPrint==false 一起滤），它们立刻变红。
//   · `vis/bidi/*`   = 逐字符测是「被隐藏」，但放进串里会挪动邻字落点，故不并入 hidden 组。
// 每类各取代表；两侧本来就一致的平局字符（U+0000 / U+0008 / U+001D）不单列。

#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#ifdef PK_TEXT_QT_ORACLE
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QString>
#include <QTransform>
using CaseImage = QImage;
using CasePoint = QPointF;
using CaseTransform = QTransform;
static constexpr const char *kBackendName = "Qt5Gui";
#else
#include "PkBrush.h"
#include "PkColor.h"
#include "PkFont.h"
#include "PkImage.h"
#include "PkImageRasterBackend.h"
#include "PkPaintCommand.h"
#include "PkPainter.h"
#include "PkPen.h"
#include "PkString.h"
using CaseImage = PkImage;
using CasePoint = PkPointF;
using CaseTransform = PkTransform;
static constexpr const char *kBackendName = "PkImageRasterBackend";
#endif

// R线-spec「对拍怎么做 · 形态契约」：防 compat 垫片把两侧编成同一个类型，
// 那样对拍恒等、永远绿。两侧类型不在同一个 TU 里，故写成「本侧确实是本侧那个真类型」。
#ifdef PK_TEXT_QT_ORACLE
static_assert(std::is_same<CaseImage, QImage>::value,
              "Qt 侧必须用真 QImage（不得被 compat 垫片替换）");
#else
static_assert(std::is_same<CaseImage, PkImage>::value,
              "Pk 侧必须用 PkImage（不得被 compat 垫片替换）");
#endif

namespace pkTextCases {

// 画布：128x64 够放下默认 12pt 与放大到 2x 的短串，且留出界余量。
inline constexpr int kCanvasWidth = 128;
inline constexpr int kCanvasHeight = 64;

struct Case {
    std::string api;        // "drawText" | "setFont+drawText"
    std::string tag;        // 由输入形态构造（R线-spec 硬教训规则一）
    // 字体：hasFont == false 时用**默认字体**（复刻两处真实调用点）。
    bool hasFont = false;
    std::string family;
    int pixelSize = -1;
    int pointSize = -1;
    // 设备空间基点（两处调用点都传「映射后的设备坐标」）。
    double px = 0.0, py = 0.0;
    // 画笔变换：先 translate(tx,ty) 再 scale(sx,sy)，与 QTransform/PkTransform 同序。
    double sx = 1.0, sy = 1.0, tx = 0.0, ty = 0.0;
    // 文字只吃笔的颜色（plan §2 P2）：笔宽/画刷对文字无影响，仍照真实调用点设置。
    unsigned penRgb = 0x000000;   // 0xRRGGBB
    double penWidth = 1.0;
    bool hasBrush = false;
    unsigned brushRgb = 0xffffff;
    std::string text;             // UTF-8
};

// FNV-1a 32：对每像素 ARGB 四字节做，顺序固定，跨平台可复现。
// 两侧都用 pixel() —— QImage::pixel() 与 PkImage::pixel() 都是打包的 0xAARRGGBB
// （pk/image/PkImage.h:100 逐字对齐 Qt）。**不要按字节读 scanLine**：那会把小端
// ARGB32 的内存布局（B,G,R,A）当逻辑顺序，两侧一旦行对齐规则不同就静默错。
inline std::uint32_t digest(const CaseImage &image)
{
    std::uint32_t hash = 2166136261u;
    const auto mix = [&hash](std::uint8_t byte) {
        hash ^= byte;
        hash *= 16777619u;
    };
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const std::uint32_t value = image.pixel(x, y);
            mix(std::uint8_t((value >> 24) & 0xff));  // A
            mix(std::uint8_t((value >> 16) & 0xff));  // R
            mix(std::uint8_t((value >> 8) & 0xff));   // G
            mix(std::uint8_t(value & 0xff));          // B
        }
    }
    return hash;
}

#ifdef PK_TEXT_QT_ORACLE
inline CaseImage makeCaseImage() { return CaseImage(kCanvasWidth, kCanvasHeight, QImage::Format_ARGB32); }
inline void fillCaseImage(CaseImage &image) { image.fill(Qt::white); }
#else
inline CaseImage makeCaseImage() { return CaseImage(kCanvasWidth, kCanvasHeight, PkImage::Format_ARGB32); }
inline void fillCaseImage(CaseImage &image) { image.fill(Pk::white); }
#endif

// 唯一的渲染实现，两侧共用。**不要在 oracle/*.cpp 或 tests/*.cpp 里再内联一份**——
// 抄第二份的结果是两边悄悄漂移，对拍就变成「自己跟自己比」。
//
// 调用形状照真实调用点：save → setTransform → setPen → (setBrush) → drawText → restore。
// Qt 侧的 setFont/drawText 映射与 libs/flake/tests/PkQPainterAdapter.cpp:117-125 逐字一致
// （PkSetFontCommand → setFont；PkDrawTextAtPointCommand → drawText(QPointF, QString)）。
inline CaseImage renderCase(const Case &c)
{
    CaseImage image = makeCaseImage();
    fillCaseImage(image);
#ifdef PK_TEXT_QT_ORACLE
    QPainter painter(&image);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    if (c.hasFont) {
        QFont font(QString::fromUtf8(c.family.c_str()));
        if (c.pixelSize > 0) font.setPixelSize(c.pixelSize);
        else if (c.pointSize > 0) font.setPointSize(c.pointSize);
        painter.setFont(font);
    }
    painter.save();
    {
        QTransform tf;
        tf.translate(c.tx, c.ty);
        tf.scale(c.sx, c.sy);
        painter.setTransform(tf);
    }
    painter.setPen(QPen(QColor(int((c.penRgb >> 16) & 0xff), int((c.penRgb >> 8) & 0xff),
                               int(c.penRgb & 0xff)), c.penWidth));
    if (c.hasBrush) {
        painter.setBrush(QBrush(QColor(int((c.brushRgb >> 16) & 0xff), int((c.brushRgb >> 8) & 0xff),
                                      int(c.brushRgb & 0xff))));
    }
    painter.drawText(QPointF(c.px, c.py), QString::fromUtf8(c.text.c_str()));
    painter.restore();
    painter.end();
#else
    PkImageRasterBackend backend(image);
    PkPainter painter(backend);
    painter.setRenderHint(PkPainter::TextAntialiasing, true);
    if (c.hasFont) {
        PkFont font(c.family);
        if (c.pixelSize > 0) font.setPixelSize(c.pixelSize);
        else if (c.pointSize > 0) font.setPointSize(c.pointSize);
        painter.setFont(font);
    }
    painter.save();
    {
        PkTransform tf;
        tf.translate(c.tx, c.ty);
        tf.scale(c.sx, c.sy);
        painter.setTransform(tf);
    }
    painter.setPen(PkPen(PkColor(int((c.penRgb >> 16) & 0xff), int((c.penRgb >> 8) & 0xff),
                                 int(c.penRgb & 0xff)), c.penWidth));
    if (c.hasBrush) {
        painter.setBrush(PkBrush(PkColor(int((c.brushRgb >> 16) & 0xff), int((c.brushRgb >> 8) & 0xff),
                                         int(c.brushRgb & 0xff))));
    }
    painter.drawText(PkPointF(c.px, c.py), PkString::fromUtf8(c.text.c_str()));
#endif
    return image;
}

// 空白画布的摘要——用来数「哪些用例其实什么都没画」（摘要 == 空图 = 恒真、无判别力）。
inline std::uint32_t emptyDigest()
{
    CaseImage image = makeCaseImage();
    fillCaseImage(image);
    return digest(image);
}

inline Case baseCase()
{
    Case c;
    c.api = "drawText";
    return c;
}

// non-ASCII 手柄名（SvgTextCursor 的 handleName 是可翻译串，实测含非 ASCII）。
inline std::vector<Case> table()
{
    std::vector<Case> cases;

    // ── 真实调用点 1：KoSvgTextShape_p_output.cpp:686 的调试字形标签 ─────────────
    // 默认字体、红笔、单位变换、位置是映射后的 viewCenter；文本 ASCII + 一个 \n。
    const char *labels[] = {"#0\n(0)", "#3~5\n(7)", "#12\n(12)"};
    for (int i = 0; i < 3; ++i) {
        Case c = baseCase();
        c.tag = std::string("callsite1/glyphdebug/") + "n" + std::to_string(i);
        c.penRgb = 0xff0000;   // Pk::red
        c.penWidth = 1.0;
        c.px = 20.0;
        c.py = 40.0;
        c.text = labels[i];
        cases.push_back(c);
    }

    // ── 真实调用点 2：SvgTextCursor.cpp:880 的手柄名 ────────────────────────────
    // 带缩放的仿射变换、画笔 + 画刷、可翻译手柄名（含非 ASCII 变体）。
    const char *names[] = {"Text Top", "テキスト上端", "Text unten"};
    const char *nameTags[] = {"ascii", "nonascii", "ascii2"};
    for (int i = 0; i < 3; ++i) {
        Case c = baseCase();
        c.tag = std::string("callsite2/handlename/scale1.5/") + nameTags[i];
        c.penRgb = 0x3f3f3f;   // bgColorForCaret(selectionColor,255) 的灰
        c.penWidth = 1.0;
        c.hasBrush = true;
        c.brushRgb = 0x2a6fd6; // selectionColor
        c.px = 30.0;
        c.py = 30.0;
        c.sx = 1.5;
        c.sy = 1.5;
        c.text = names[i];
        cases.push_back(c);
    }

    // ── 对抗用例：边界与退化 ───────────────────────────────────────────────────
    struct Extra {
        const char *tag;
        const char *text;
        unsigned penRgb;
        double penWidth;
        double sx, sy, tx, ty;
        double px, py;
    };
    const Extra extras[] = {
        {"adv/empty",            "",                0x000000, 1.0, 1.0,  1.0,  0.0, 0.0, 20.0, 40.0},
        {"adv/newline-only",     "\n",              0x000000, 1.0, 1.0,  1.0,  0.0, 0.0, 20.0, 40.0},
        {"adv/newline-mid",      "X\nY",            0x000000, 1.0, 1.0,  1.0,  0.0, 0.0, 20.0, 40.0},
        // 与 newline-mid 同位置同笔、只差一个 `\n`：Qt 的点重载把 `\n` 整个丢弃
        // （plan §2 P1，本机复测 ndiff=0），所以 Qt 侧这两例**必须逐像素相同**。
        // ⚠ 实测：**Pk 侧不是**——Pk 把 `\n` 当有推进量的字形（约 4px），两例摘要
        //   不同。这是本工装发现的**真实分歧**，不是用例写错：真实调用点 1 的文本
        //   就是 "#0\n(0)"。详见 pk/render/README.md 与本任务报告。
        {"adv/newline-twin",     "XY",              0x000000, 1.0, 1.0,  1.0,  0.0, 0.0, 20.0, 40.0},
        {"adv/newline-multi",    "ABC\nDEF\nGHI",   0x000000, 1.0, 1.0,  1.0,  0.0, 0.0, 10.0, 40.0},
        {"adv/nonascii",         "Hello, \xe4\xb8\x96\xe7\x95\x8c", 0x000000, 1.0, 1.0, 1.0, 0.0, 0.0, 10.0, 40.0},
        {"adv/scale-int2",       "Ab",              0xff0000, 1.0, 2.0,  2.0,  0.0, 0.0, 10.0, 30.0},
        {"adv/scale-frac1.5",    "Ab",              0x000000, 1.0, 1.5,  1.5,  0.0, 0.0, 20.0, 40.0},
        {"adv/scale-frac0.67",   "Ab",              0x000000, 1.0, 0.67, 0.67, 0.0, 0.0, 20.0, 40.0},
        {"adv/scale+translate",  "Wg",              0x000000, 1.0, 1.75, 1.75, 5.0, 8.0, 12.0, 20.0},
        // 笔宽 1 vs 7，其余全同：文字只吃笔的颜色（plan §2 P2），二者摘要必须相同。
        {"adv/pen-width-1",      "Ab",              0x000000, 1.0, 1.0,  1.0,  0.0, 0.0, 20.0, 40.0},
        {"adv/pen-width-7",      "Ab",              0x000000, 7.0, 1.0,  1.0,  0.0, 0.0, 20.0, 40.0},
        {"adv/pen-blue",         "Ab",              0x0000ff, 1.0, 1.0,  1.0,  0.0, 0.0, 20.0, 40.0},
        {"adv/fractional-pos",   "Ab",              0x000000, 1.0, 1.0,  1.0,  0.0, 0.0, 30.5, 40.25},
        {"adv/offcanvas",        "Ab",              0x000000, 1.0, 1.0,  1.0,  0.0, 0.0, 200.0, 200.0},
    };
    for (const auto &e : extras) {
        Case c = baseCase();
        c.tag = e.tag;
        c.text = e.text;
        c.penRgb = e.penRgb;
        c.penWidth = e.penWidth;
        c.sx = e.sx; c.sy = e.sy; c.tx = e.tx; c.ty = e.ty;
        c.px = e.px; c.py = e.py;
        cases.push_back(c);
    }

    // ── 可见性规则扫描：Qt 丢弃的类别 vs 保留的类别（口径见文件头）──────────────────
    // 形式统一为 "A<c>B"：单测该字符本身的推进量/墨迹，且一旦被丢掉就与 "AB" 同摘要。
    struct VisCase {
        const char *tag;
        const char *text;
    };
    const VisCase vis[] = {
        // Qt 的 applyVisibilityRules 集合（qtextengine.cpp:1361）——**本轮修的**。
        // 前三条是实测会分歧的（非 Default_Ignorable），后三条 Pk 早已靠 HarfBuzz
        // 隐藏、滤掉是空操作，一并保留以对齐规则的完整定义。
        {"vis/hidden/lf",       "A\012B"},
        {"vis/hidden/lf-twin",  "AB"},          // 与上一条同位置同笔，只差一个 \n
        {"vis/hidden/ff",       "A\014B"},
        {"vis/hidden/cr",       "A\015B"},
        {"vis/hidden/ls",       "A\342\200\250B"},
        {"vis/hidden/ps",       "A\342\200\251B"},
        {"vis/hidden/shy",      "A\302\255B"},
        // 其余被两侧一致隐藏的类别（Cf / bidi）——反向守卫：滤除集合若开大，这里可能连带变红。
        {"vis/hidden/zwsp",     "A\342\200\213B"},
        {"vis/hidden/rlo",      "A\342\200\256B"},
        {"vis/hidden/wj",       "A\342\201\240B"},
        {"vis/hidden/bom",      "A\357\273\277B"},
        // 被隐藏**但会挪动邻字落点**的 bidi 控制符：Qt 视作完全无操作（实测 "A<c>B" 的
        // advance 与墨迹定界与 "AB" 逐像素相同），Pk 走 Raqm 的 bidi 解析后落点不同 ⇒
        // 这两条的摘要**不等于** vis/hidden/lf-twin。与 vis/kept/tab 同属「同一层、不同
        // 实现」，本批不修（见报告 §5）。实测两者与 lf-twin 的摘要都不同（见 golden）。
        // 同一现象还见于 U+2067/U+2068（RSI/FSI）与 U+2069（PDI），本组各取一代表。
        {"vis/bidi/lri",        "A\342\201\246B"},
        {"vis/bidi/alm",        "A\330\234B"},
        // Qt 保留的类别（推进非 0）——**不得**被滤掉；\t 还是本批登记的待裁决分歧。
        {"vis/kept/tab",        "A\011B"},
        {"vis/kept/vt",         "A\013B"},
        {"vis/kept/space",      "A B"},
        {"vis/kept/nbsp",       "A\302\240B"},
        {"vis/kept/ideospace",  "A\343\200\200B"},
        {"vis/kept/del",        "A\177B"},
        {"vis/kept/mvs",        "A\341\240\216B"},
    };
    for (const auto &v : vis) {
        Case c = baseCase();
        c.tag = v.tag;
        c.text = v.text;
        c.penRgb = 0x000000;
        c.penWidth = 1.0;
        c.px = 20.0;
        c.py = 40.0;
        cases.push_back(c);
    }

    // ── 显式 setFont 用例：覆盖 PkSetFontCommand（两处真实调用点都没用到它）────────
    struct FontCase {
        const char *tag;
        const char *family;
        int pixelSize;
        int pointSize;
        const char *text;
        double sx;
        double px, py;
    };
    const FontCase fonts[] = {
        {"setfont/dejavu-px24",   "DejaVu Sans", 24, -1, "Wg",  1.0, 10.0, 40.0},
        {"setfont/dejavu-pt18",   "DejaVu Sans", -1, 18, "Wg",  1.0, 10.0, 40.0},
        {"setfont/dejavu-px24-s", "DejaVu Sans", 24, -1, "Wg",  1.5, 10.0, 30.0},
    };
    for (const auto &f : fonts) {
        Case c = baseCase();
        c.api = "setFont+drawText";
        c.tag = f.tag;
        c.hasFont = true;
        c.family = f.family;
        c.pixelSize = f.pixelSize;
        c.pointSize = f.pointSize;
        c.penRgb = 0x000000;
        c.penWidth = 1.0;
        c.sx = f.sx;
        c.sy = f.sx;
        c.px = f.px;
        c.py = f.py;
        c.text = f.text;
        cases.push_back(c);
    }

    return cases;
}

} // namespace pkTextCases
