// R-55 Task 5 · 判据②「真实调用点试接」的依赖墙 driver。
//
// ⚠ 本文件**不是**真实测试类，**也不是**真实调用点本身。它是「复刻两处真实
//   drawText 调用点代码形状的 driver」，走 R线-spec〈依赖墙挡住真实测试类时〉
//   那条降级路径。形态照抄先例 `pk/time/tests/graft/date_parser_driver.cpp`
//   （R-16 Task 3）：独立 main()、只链本模块薄壳库、不接 pk/test harness。
//
// ── 复刻对象（本 worktree 现场逐行核对，2026-09-12）─────────────────────────
//   ① libs/flake/text/KoSvgTextShape_p_output.cpp:686
//      `KoSvgTextShape::Private::paintDebug`（:604 起）里的字形索引标签。
//      源码逐字（:667-687）：
//
//          const PkPointF center = tf.mapRect(result.at(i).layoutBox()).center();
//          PkString text = "#";
//          text += PkString::number(i);
//          {
//              // Find the range of this typographic character
//              int end = i + 1;
//              while (end < result.size() && result[end].middle) {
//                  end++;
//              }
//              end--;
//              if (end != i) {
//                  text += "~";
//                  text += PkString::number(end);
//              }
//          }
//          text += PkString("\n(%1)").arg(result.at(i).plaintTextIndex);
//          const PkPointF viewCenter = painter.transform().map(center);
//          painter.save();
//          painter.setTransform(PkTransform());
//          painter.setPen(PkPen(Pk::red));
//          painter.drawText(viewCenter, text);
//          painter.restore();
//
//      下面 drvCallsite1_glyphLabel() 就是这一段，**语句、参数个数/类型、
//      调用顺序逐条照抄**；只有两条边界被换成形参：
//        · `result` —— 真实代码里是 `PkVector<Private::TextChunk>`，本 driver 只用
//          到 `.size()/.middle/.plaintTextIndex` 三个访问，故用同形的局部容器承载
//          （见 ChunkRef）。文字排版闭包（`tf.mapRect(layoutBox())`）不在
//          判据②的范围里，故 `center` 直接作形参传入。
//        · 渲染准备（PkImageRasterBackend/PkPainter 的构造与 render hint）——真实
//          调用点是管线里被传进来的 painter；这里按 oracle 用例表的同一套准备。
//
//   ② plugins/tools/svgtexttool/SvgTextCursor.cpp:880
//      `SvgTextCursor::paintDecorations`（:791 起）里的排版手柄名。源码逐字
//      （:795 / :830 / :872-882）：
//
//          gc.setTransform(d->shape->absoluteTransformation(), true);   // :795
//          ...
//          PkTransform painterTf = gc.transform();                      // :830
//          ...
//          PkString name = handleName(d->hoveredTypeSettingHandle);     // :872
//          if (!name.isEmpty()) {
//              gc.save();
//              PkPen pen(bgColorForCaret(selectionColor, 255));
//              pen.setCosmetic(true);
//              pen.setWidth(decorationThickness);
//              gc.setPen(pen);
//              gc.setBrush(PkBrush(selectionColor));
//              gc.drawText(painterTf.map(d->typeSettingDecor.closestBaselinePoint), name);
//              gc.restore();
//          }
//
//      下面 drvCallsite2_handleName() 就是这一段。`handleName()` 是 :428 起的
//      一长串 `i18nc` 手柄名表，本 driver 取其**返回值**作形参（`name`）——
//      复刻的是 drawText 调用点的形状，不是那个名字表。
//
// ── 第 4 条：指名是哪堵墙、为什么在 locks 之外（落地时自己复核过坐标）──────
//   · ① 的文件编进 target **kritaflake**：
//     `libs/flake/CMakeLists.txt:563` `kis_add_library(kritaflake SHARED ...)`，
//     该源文件在 `:287` 与 `:494` 各列一次。
//   · ② 的文件编进 target **krita_tool_svgtext_static**：
//     `plugins/tools/svgtexttool/CMakeLists.txt:49`
//     `kis_add_library(krita_tool_svgtext_static STATIC ${SvgTextTool_SRCS})`。
//   这两个 target 的 **CMake 生成产物**（`generate_export_header` 出的
//   `kritaflake_export.h` @ `libs/flake/CMakeLists.txt:574`、
//   `kritatoolsvgtext_export.h` @ `plugins/tools/svgtexttool/CMakeLists.txt:51`、
//   ECM 宏），以及它们 `PUBLIC` 传递闭包里的 `kritaimage`/`kritaui`，**都不在
//   R-55 的 locks 内**（locks = `pk/render`、`pk/font`、
//   `libs/flake/PkImageRasterBackend.cpp`、`libs/flake/text`、
//   `plugins/tools/svgtexttool`）。
//   ⚠ **锁里有 `libs/flake/text` 与 `plugins/tools/svgtexttool`，不等于能编出这两个
//   target**——那是「允许改这两个目录」，而编出 target 要的是它自己的生成头与整条
//   依赖闭包。拆这堵墙是后续把 `libs/flake` / 该插件剥完 Qt 的 S 任务的事
//   （不是 R-55，R-55 只交付 `pk/render` + `pk/font` 两个薄壳库）。
//   ⇒ 本任务范围内**物理编不过**真实测试类。实测原始报错贴在
//   `.superpowers/sdd/R-55/task-5-report.md` §2（分层 `-fsyntax-only` 探针）。
//
// ── 第 2 条：校验值来自对真 Qt 的实测探针 —— **本机给不出，如实说明** ────────
//   本机 Qt 二进制里 **一个 QFontEngineFT 都没有**（`QFontEngineFT=0 of
//   nm_lines=9594`，`pk/font/oracle/compare.cmake` 与本目录 `run_text.sh` 各自
//   独立探到），文字光栅化的参照系在本机不存在 ⇒ **本 driver 没有任何
//   「与真 Qt 对拍」的结论**。它下面自报的数字全部是 **Pk 侧自产**，不得当 Qt 金标
//   读（〈文字类对拍〉通则 2026-09-12 人已裁决；plan §6 第 1、6 条）。
//   本 driver 能做的、也确实做了的「校验」有两类：
//     (a) **同引擎交叉核对**：callsite1 的输入与 oracle 用例表
//         `callsite1/glyphdebug/n{0,1,2}` 逐项相同 ⇒ 本 driver 的摘要必须等于
//         `oracle/text_golden.txt` 里那三行。这证明「逐行复刻的 driver」与
//         「oracle 用例表」算的是同一个场景，不是两个臆想场景。
//     (b) **复刻代码内部不变量**：见 main() 的 ASSERT 段。
//
// ── 第 3 条：显式标注替代品 ────────────────────────────────────────────────
//   本注释 + 每个复刻函数自己的注释 + task-5-report.md §3。运行时第一条 stdout
//   就是 DRIVER-NOTICE，读输出的人不可能把它误读成真实测试类。
//
// 跑法：`text_draw_driver`（不接 ctest —— 它不是判据，是判据② 的试接证据；
//       R-16 先例同）。期望值从 `PK_RENDER_TEXT_GOLDEN` 指向的 golden 文件读。
//       FONTCONFIG_PATH 必须设（plan §6 第 5 条），否则字体回退会漂。

#include "PkBrush.h"
#include "PkColor.h"
#include "PkImage.h"
#include "PkImageRasterBackend.h"
#include "PkPainter.h"
#include "PkPen.h"
#include "PkPoint.h"
#include "PkString.h"
#include "PkTransform.h"

#include <kis_painting_tweaks.h>   // 真实生产头：本 driver 直接编 libs/flake/kis_painting_tweaks.cpp

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ─────────────────────────────────────────────────────────────────────────────
// 真实代码里 `result` 的元素（`KoSvgTextShape::Private::TextChunk`）本 driver 用到
// 的字段。同名、同取法，只是容器换成 std::vector（形状复刻不涉及容器实现）。
struct ChunkRef {
    bool middle;
    int plaintTextIndex;
};

// ═════════════════════════════════════════════════════════════════════════════
// 复刻 ①：libs/flake/text/KoSvgTextShape_p_output.cpp:686
//         （KoSvgTextShape::Private::paintDebug 的字形索引标签）
//
// ⚠ 这不是 KoSvgTextShape_p_output.cpp，是复刻它调用点形状的 driver —— 该文件
//   编在 target kritaflake 里，其 CMake 生成产物不在 R-55 的 locks 内。
// ═════════════════════════════════════════════════════════════════════════════
void drvCallsite1_glyphLabel(PkPainter &painter,
                             const std::vector<ChunkRef> &result,
                             int i,
                             const PkPointF &center /* 真实代码：tf.mapRect(result.at(i).layoutBox()).center() */)
{
    // ↓↓↓ 以下到 painter.restore() 为止，逐行照抄 KoSvgTextShape_p_output.cpp:667-687 ↓↓↓
    PkString text = "#";
    text += PkString::number(i);
    {
        // Find the range of this typographic character
        int end = i + 1;
        while (end < result.size() && result[end].middle) {
            end++;
        }
        end--;
        if (end != i) {
            text += "~";
            text += PkString::number(end);
        }
    }
    text += PkString("\n(%1)").arg(result.at(i).plaintTextIndex);
    const PkPointF viewCenter = painter.transform().map(center);
    painter.save();
    painter.setTransform(PkTransform());
    painter.setPen(PkPen(Pk::red));
    painter.drawText(viewCenter, text);
    painter.restore();
    // ↑↑↑ 照抄结束 ↑↑↑
}

// ═════════════════════════════════════════════════════════════════════════════
// 复刻 ②：plugins/tools/svgtexttool/SvgTextCursor.cpp:880
//         （SvgTextCursor::paintDecorations 的排版手柄名）
//
// ⚠ 这不是 SvgTextCursor.cpp，是复刻它调用点形状的 driver —— 该文件编在 target
//   krita_tool_svgtext_static 里，其 CMake 生成产物不在 R-55 的 locks 内。
//
// bgColorForCaret() 逐字照抄同文件 :786-789（4 行，是 ② 调用点的直接上下文）：
//   static PkColor bgColorForCaret(const PkColor &c, int opacity = 64) {
//       return KisPaintingTweaks::luminosityCoarse(c) > 0.8 ? PkColor(0, 0, 0, opacity) : PkColor(255, 255, 255, opacity);
//   }
// 其中 `KisPaintingTweaks::luminosityCoarse` **不抄**——本 driver 直接把真实源文件
// libs/flake/kis_painting_tweaks.cpp 编进来（见 CMakeLists 与 `#include
// <kis_painting_tweaks.h>`），用的是树里那一份真实实现。
// ═════════════════════════════════════════════════════════════════════════════
PkColor bgColorForCaret(const PkColor &c, int opacity = 64)
{
    return KisPaintingTweaks::luminosityCoarse(c) > 0.8 ? PkColor(0, 0, 0, opacity) : PkColor(255, 255, 255, opacity);
}

void drvCallsite2_handleName(PkPainter &gc,
                             const PkColor &selectionColor,
                             int decorationThickness,
                             const PkPointF &closestBaselinePoint /* 真实代码：d->typeSettingDecor.closestBaselinePoint */,
                             const PkString &name /* 真实代码：handleName(d->hoveredTypeSettingHandle) */,
                             const PkTransform &shapeAbsoluteTransformation /* 真实代码：d->shape->absoluteTransformation() */)
{
    // ↓↓↓ 照抄 SvgTextCursor.cpp:795（外层 save 之后立刻设的变换）↓↓↓
    gc.save();
    gc.setTransform(shapeAbsoluteTransformation, true);
    // ↑↑↑

    // ↓↓↓ 照抄 :830（在同一个变换生效期间取样）↓↓↓
    PkTransform painterTf = gc.transform();
    // ↑↑↑

    // ↓↓↓ 照抄 :872-882 ↓↓↓
    if (!name.isEmpty()) {
        gc.save();
        PkPen pen(bgColorForCaret(selectionColor, 255));
        pen.setCosmetic(true);
        pen.setWidth(decorationThickness);
        gc.setPen(pen);
        gc.setBrush(PkBrush(selectionColor));
        gc.drawText(painterTf.map(closestBaselinePoint), name);
        gc.restore();
    }
    // ↑↑↑ 照抄结束 ↑↑↑

    gc.restore();   // 对应 :885 的 gc.restore()
}

// ── 渲染准备：与 oracle 用例表 renderCase() 同一套（白底、TextAntialiasing）──
PkImage makeCanvas(Pk::GlobalColor ground)
{
    PkImage image(128, 64, PkImage::Format_ARGB32);
    image.fill(ground);
    return image;
}

std::uint32_t digestOf(const PkImage &image)
{
    // 与 oracle/text_cases.h 的 digest() 同一算法、同一取法（pixel()，非 scanLine）
    std::uint32_t hash = 2166136261u;
    const auto mix = [&hash](std::uint8_t byte) {
        hash ^= byte;
        hash *= 16777619u;
    };
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const std::uint32_t value = image.pixel(x, y);
            mix(std::uint8_t((value >> 24) & 0xff));
            mix(std::uint8_t((value >> 16) & 0xff));
            mix(std::uint8_t((value >> 8) & 0xff));
            mix(std::uint8_t(value & 0xff));
        }
    }
    return hash;
}

int g_fail = 0;

void check(bool cond, const std::string &what)
{
    std::cout << (cond ? "  OK   " : "  FAIL ") << what << '\n';
    if (!cond)
        ++g_fail;
}

template <typename T>
std::string hex32(T v)
{
    std::ostringstream os;
    os << std::hex << std::uint32_t(v);
    return os.str();
}

// 从 golden 文件里取某一条 DIFFTAG 的摘要值。
std::map<std::string, std::uint32_t> loadGolden(const std::string &path)
{
    std::map<std::string, std::uint32_t> out;
    std::ifstream in(path);
    for (std::string line; std::getline(in, line);) {
        std::istringstream is(line);
        std::string kind, api, tag;
        std::uint32_t value = 0;
        if (is >> kind >> api >> tag >> value && kind == "DIFFTAG")
            out[tag] = value;
    }
    return out;
}

} // namespace

int main(int argc, char **argv)
{
    std::cout << "DRIVER-NOTICE 本可执行体不是真实测试类：它逐行复刻两处真实 drawText "
                 "调用点的代码形状\n";
    std::cout << "DRIVER-NOTICE 复刻对象：libs/flake/text/KoSvgTextShape_p_output.cpp:686 与 "
                 "plugins/tools/svgtexttool/SvgTextCursor.cpp:880\n";
    std::cout << "DRIVER-NOTICE 原因：这两个文件编在 kritaflake / krita_tool_svgtext_static 里，"
                 "其 CMake 生成产物（*_export.h、ECM 宏、kritaimage/kritaui 闭包）不在 R-55 的 locks 内\n";
    std::cout << "DRIVER-NOTICE 本机 Qt 无 FreeType 文字引擎（QFontEngineFT=0），"
                 "下列数字全部是 Pk 侧自产，不是 Qt 金标\n";

    const char *goldenPath = std::getenv("PK_RENDER_TEXT_GOLDEN");
    const std::string golden = goldenPath ? goldenPath : "pk/render/oracle/text_golden.txt";
    const auto expected = loadGolden(golden);
    std::cout << "DRIVER-GOLDEN " << golden << " entries=" << expected.size() << '\n';

    // ═══ ① 复刻调用点 1 ═══════════════════════════════════════════════════════
    // 输入与 oracle 用例表 callsite1/glyphdebug/n{0,1,2} 逐项相同：
    //   默认字体、红笔、外层变换 = 单位阵、中心 (20,40)、
    //   文本依次 "#0\n(0)" / "#3~5\n(7)" / "#12\n(12)"。
    struct C1 {
        const char *tag;
        int i;
        std::vector<ChunkRef> result;
    };
    // 三组 `result` 造得让下面那段 `while (end < result.size() && result[end].middle)`
    // 走到与 oracle 用例表 labels[] 逐字相同的文本：
    //   n0 → "#0\n(0)"    i=0，无续接 chunk ⇒ end==i ⇒ 不带 "~"
    //   n1 → "#3~5\n(7)"  i=3，idx 4/5 是 middle=true 的续接、idx 6 结束 ⇒ end=5
    //   n2 → "#12\n(12)"  i=12，无续接 ⇒ 不带 "~"
    const C1 cases1[] = {
        {"callsite1/glyphdebug/n0", 0, {{false, 0}}},
        {"callsite1/glyphdebug/n1", 3,
         {{false, 7}, {false, 7}, {false, 7}, {false, 7}, {true, 7}, {true, 7}, {false, 7}}},
        {"callsite1/glyphdebug/n2", 12,
         {{false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0},
          {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 0}, {false, 12}}},
    };
    for (const auto &c : cases1) {
        PkImage image = makeCanvas(Pk::white);
        PkImageRasterBackend backend(image);
        PkPainter painter(backend);
        painter.setRenderHint(PkPainter::TextAntialiasing, true);
        drvCallsite1_glyphLabel(painter, c.result, c.i, PkPointF(20.0, 40.0));
        const std::uint32_t got = digestOf(image);
        const auto it = expected.find(c.tag);
        const bool has = it != expected.end();
        std::cout << "C1 " << c.tag << " digest=" << got
                  << " golden=" << (has ? std::to_string(it->second) : std::string("(absent)"));
        if (has) {
            check(got == it->second, std::string("callsite1 ") + c.tag + " == oracle 用例表同一输入");
        } else {
            // 「没跑到」不是「跑绿了」：golden 读不到这一条就是失败，不放过。
            check(false, std::string("callsite1 ") + c.tag + " 在 golden 里缺席（golden 没读到？）");
        }
    }

    // ═══ ② 复刻调用点 2 ═══════════════════════════════════════════════════════
    // 复刻 :795 的 `setTransform(absoluteTransformation, true)`：用例表以
    // scale(1.5) 作 absoluteTransformation；:880 的点是 `painterTf.map(baselinePoint)`。
    // ⚠ 注意真实语义：:795 之后**没有**把变换重置成单位阵，:880 传的又是**已经映射过**
    //   的点 ⇒ PkImageRasterBackend::drawText 里 `m_state.transform.map(command.position)`
    //   会把那个点**再映射一次**（PkImageRasterBackend.cpp:1262）。本 driver 照实复刻，
    //   不做"修正"——真实调用点就是双重映射。
    PkTransform shapeTf;
    shapeTf.scale(1.5, 1.5);

    const PkColor selectionColor(0x2a, 0x6f, 0xd6);   // 与 oracle 用例表的 selectionColor 同值
    const PkPointF closestBaselinePoint(20.0, 20.0);  // painterTf.map → (30,30)，与用例表同值
    const int decorationThickness = 1;

    // 复刻代码算出来的笔色 —— 这就是真实调用点会用的颜色
    const PkColor computedPen = bgColorForCaret(selectionColor, 255);
    std::cout << "C2 penColor bgColorForCaret(selectionColor=2a6fd6, 255) = "
              << hex32(computedPen.rgba()) << '\n';
    check(computedPen.rgba() == 0xffffffffu,
          "bgColorForCaret 对本用例的 selectionColor 返回**白**（0xffffffff），"
          "不是 oracle 用例表注释里写的灰 0x3f3f3f —— 见报告 §4.2");

    const char *names[3] = {"Text Top", "\xe3\x83\x86\xe3\x82\xad\xe3\x82\xb9\xe3\x83\x88\xe4\xb8\x8a\xe7\xab\xaf", "Text unten"};
    const char *tags[3] = {"callsite2/ascii", "callsite2/nonascii", "callsite2/ascii2"};
    const std::uint32_t blankWhite = digestOf(makeCanvas(Pk::white));
    for (int k = 0; k < 3; ++k) {
        // (a) 忠实复刻：真实调用点的笔色（对白底画布 ⇒ 白字，必然看不见）
        PkImage image = makeCanvas(Pk::white);
        PkImageRasterBackend backend(image);
        PkPainter gc(backend);
        gc.setRenderHint(PkPainter::TextAntialiasing, true);
        drvCallsite2_handleName(gc, selectionColor, decorationThickness, closestBaselinePoint,
                                PkString::fromUtf8(names[k]), shapeTf);
        const std::uint32_t got = digestOf(image);
        std::cout << "C2 " << tags[k] << " faithful digest=" << got
                  << " blank=" << (got == blankWhite ? "yes" : "no") << '\n';
        check(got == blankWhite,
              std::string("callsite2 ") + tags[k] +
                  " 忠实复刻在白底上 == 空图（真实调用点的笔色是白，白底不可见）");

        // (b) 同一段复刻代码 + 深底：证明它不是"什么都没画"，只是白字在白底上看不见
        PkImage dark = makeCanvas(Pk::black);
        PkImageRasterBackend darkBackend(dark);
        PkPainter darkGc(darkBackend);
        darkGc.setRenderHint(PkPainter::TextAntialiasing, true);
        drvCallsite2_handleName(darkGc, selectionColor, decorationThickness, closestBaselinePoint,
                                PkString::fromUtf8(names[k]), shapeTf);
        const std::uint32_t darkDigest = digestOf(dark);
        std::cout << "C2 " << tags[k] << " darkground digest=" << darkDigest
                  << " blank=" << (darkDigest == digestOf(makeCanvas(Pk::black)) ? "yes" : "no") << '\n';
        check(darkDigest != digestOf(makeCanvas(Pk::black)),
              std::string("callsite2 ") + tags[k] + " 深底上有墨（调用点形状确实画到了后端）");
    }

    std::cout << "DRIVER-RESULT " << (g_fail == 0 ? "ok" : "FAIL") << " failures=" << g_fail << '\n';
    return g_fail == 0 ? 0 : 1;
}
