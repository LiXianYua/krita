// R-54 Task 3 · 判据②「真实调用点试接」的依赖墙 driver（批 2：PkSvgPainterBackend）。
//
// ⚠ 本文件**不是**真实测试文件，**也不是**真实调用点本身。它是「复刻
//   `libs/flake/svg/SvgWriter.cpp:240-277`（`SvgWriter::saveGeneric`）代码形状的 driver」，
//   走 R线-spec〈依赖墙挡住真实测试类时〉那条降级路径。形态照抄先例
//   `pk/render/tests/graft/text_draw_driver.cpp`（R-55）与
//   `pk/time/tests/graft/date_parser_driver.cpp`（R-16）：独立 `main()`、只链本模块薄壳库
//   `pkrender`、不接 `pk/test` harness、**也不 `add_test`**——它是判据② 的试接证据，
//   不是一条判据。
//
// ── 第 1 条：逐行复刻真实调用点的代码形状 ────────────────────────────────────
//   复刻对象（本 worktree 现场逐行核对，2026-09-12）：`libs/flake/svg/SvgWriter.cpp:242-254`。
//   源码逐字：
//
//       const PkRectF bbox = shape->boundingRect();
//
//       // paint shape to the image
//       KoShapePainter painter;
//       painter.setShapes(PkList<KoShape*>()<< shape);
//
//       const PkRect viewport = SvgUtil::toUserSpace(bbox).toRect();
//       PkSvgPainterBackend backend;
//       PkPainter svgPainter(backend);
//       painter.paint(svgPainter, viewport, bbox);
//       const std::string svgData = backend.document();
//
//   **真实 SVG 消费路径**（同文件 :255-277，plan 要求「在注释里写清」）：`svgData` 拿到后
//   `saveGeneric` 二选一——
//     · 非空（:272-276）：
//           PkMemoryStream stream;
//           stream.open(PkStream::ReadWrite);
//           stream.write(svgData.data(), static_cast<PkStream::pk_int64>(svgData.size()));
//           stream.seek(0);
//           context.shapeWriter().addCompleteElement(&stream);
//       —— 即「后端产出的**整份 SVG 文档**原样嵌进外层 SVG 文档」（不是逐元素翻译）。
//     · 空（:256-271）：回退栅格——按两倍 bbox 尺寸造透明 `PkImage`、`painter.paint(image)`、
//       再写一个 `<image ... xlink:href=...>` 元素。
//   本 driver 复刻 `:251-254` 那段（后端默认构造 + PkPainter + `document()`），并把
//   「判空 → 两条分支」这个骨架也复刻出来（见 main() 的 DRIVER-FALLBACK 段）；但**不**复刻
//   `addCompleteElement`/`shapeWriter()` 那一串——它要 `SvgSavingContext`，那套在
//   `libs/flake/svg` 之外（见第 4 条的墙）。
//
//   本 driver 与真实代码的两处**边界替换**（做法同 text_draw_driver，逐条列出，不含糊）：
//     · `KoShapePainter painter; painter.setShapes(...); painter.paint(svgPainter, viewport, bbox);`
//       —— KoShapePainter 会把 shape 的 pen/brush/transform 翻译成逐个原语命令 dispatch 给
//       backend。判据② 只压到 `PkSvgPainterBackend` 这一层，故这里用**同形的 PkPainter 命令**
//       直接表达那串 dispatch（`setRenderHint`/`setPen`/`setBrush`/`draw*`，次序与 oracle
//       用例表的 `documentFor()` 一致）。`bbox`/`viewport` 随之由共享用例表的 `Case` 承载
//       （本 driver 的「shape」就是用例表里那条 Case）。
//     · **不是**替换的关键复刻点：后端仍然按真实代码**默认构造**
//       （`PkSvgPainterBackend backend;`，无 bounds）——它决定产出的 `<svg>` 无
//       `width/height/viewBox`（真 Qt 的 `QSvgGenerator` 不 `setSize` 时同形）。这一点在
//       oracle 用例表里是**不同**的：`documentFor()` 传 `canvasRect()`（固定 32×32 画布，
//       为逐像素对拍而设）。两处的差别与后果见 task-3-report.md §3。
//
// ── 第 2 条：校验值来自对真 Qt 的实测探针 ────────────────────────────────────
//   driver 是 Qt-free 的，**算不出像素摘要**，所以它自己不造期望值。它自报的数字只有两类：
//     (a) 每个用例文档的 FNV-1a 摘要（`DRIVER-DOC-HASH` 行）+ 文档是否为空；
//     (b) 结构不变量断言（`DRIVER-CHECK`）：pen 非 NoPen 的用例文档必须非空、
//         `!m_supported`（不支持的 brush）的用例文档必须为空。这是**复刻代码自身的不变量**，
//         不是「与 Qt 一致」的结论。
//   真正的「与真 Qt 对不对得上」由一支**手工编的真 Qt 探针**给出：探针吃本 driver 的 stdout，
//   逐例用**同一个** `QSvgRenderer` 渲染「本 driver 的文档」与「真 Qt `QSvgGenerator` 同形
//   （同样不 setSize）产出的参照文档」，逐像素比。**探针命令与原始输出见
//   `.superpowers/sdd/R-54/task-3-report.md` §3**，期望值不是猜的、也不是抄文档的。
//
// ── 第 3 条：显式标注自己是替代品 ────────────────────────────────────────────
//   本注释 + 运行时 stdout 头四行 `DRIVER-NOTICE`。读输出的人不可能把它误读成真实测试文件。
//
// ── 第 4 条：指名依赖墙、为什么在 locks 之外 ─────────────────────────────────
//   `libs/flake/svg/SvgWriter.cpp` 编进 target **`kritaflake`**。R-54 持有
//   `libs/flake/svg` **目录**的锁——那只说明「可以改这个目录里的文件」，**不等于**能编
//   `kritaflake` 这个 target：它的构建要自己的 CMake 生成产物（`kritaflake_export.h`、
//   ECM 宏）与 `PUBLIC` 私有闭包（`kritaimage`/`kritaui` 等），都不在 R-54 的 locks 内。
//   与 R-55 对 `kritaflake` 的结论同源。拆这堵墙的任务 = 把 `libs/flake` 整块剥完 Qt 的
//   S 任务；在那之前，本层 driver 证据已被 R线-spec〈依赖墙挡住真实测试类时〉接受为完成判据。
//
// 跑法：`svg_backend_driver > /tmp/driver-out.txt`，再把 `driver-out.txt` 喂给真 Qt 探针
//       （不接 ctest，理由同 R-16/R-55 先例）。无任何环境变量依赖。

#include "svg_primitive_cases.h"  // 与 oracle/ctest 共用同一份输入表（几何来自它）

#include "PkBrush.h"
#include "PkColor.h"
#include "PkGradient.h"
#include "PkPainter.h"
#include "PkPaintCommand.h"
#include "PkPen.h"
#include "PkPoint.h"
#include "PkSvgPainterBackend.h"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool cond, const std::string &what)
{
    if (!cond) {
        std::cout << "DRIVER-CHECK FAIL: " << what << '\n';
        ++g_failures;
    } else {
        std::cout << "DRIVER-CHECK PASS: " << what << '\n';
    }
}

std::uint32_t fnv1a(const std::string &bytes)
{
    std::uint32_t hash = 2166136261u;
    for (unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

std::string hex32(std::uint32_t value)
{
    std::ostringstream out;
    out << std::hex << value;
    return out.str();
}

// ═════════════════════════════════════════════════════════════════════════════
// 复刻 `SvgWriter::saveGeneric` 的 `:251-254`（见文件头第 1 条）。
// 逐字对齐的只有那四行的**形状**：`PkSvgPainterBackend backend;` 默认构造、
// `PkPainter svgPainter(backend);`、一串画笔设置 + 原语 dispatch、`backend.document()`。
// ⚠ 这不是 `SvgWriter.cpp`——该文件编在 target `kritaflake` 里，其 CMake 生成产物不在
//   R-54 的 locks 内（见文件头第 4 条）。
// ═════════════════════════════════════════════════════════════════════════════
std::string drvSaveGenericShape(const pkSvgCases::Case &shape)
{
    // ↓↓↓ 与 SvgWriter.cpp:251-254 同形 ↓↓↓
    PkSvgPainterBackend backend;       // :251 真实代码：`PkSvgPainterBackend backend;`（默认 ctor）
    PkPainter svgPainter(backend);     // :252 真实代码：`PkPainter svgPainter(backend);`
    // :253 真实代码：`painter.paint(svgPainter, viewport, bbox);` —— KoShapePainter 按 shape
    // 的画笔状态 dispatch 下面这几条原语命令（次序同 oracle 用例表的 documentFor()）。
    svgPainter.setRenderHint(PkPainter::RenderHint::Antialiasing, true);
    svgPainter.setPen(shape.hasPen ? PkPen(PkColor(Pk::black), shape.penWidth) : PkPen(Pk::NoPen));
    svgPainter.setBrush(shape.hasBrush ? PkBrush(PkColor(Pk::red)) : PkBrush(Pk::NoBrush));
    switch (shape.kind) {
    case pkSvgCases::Kind::Ellipse:
        svgPainter.drawEllipse(shape.rect);
        break;
    case pkSvgCases::Kind::Polygon: {
        PkPolygonF polygon;
        for (const auto &p : shape.points) polygon.append(p);
        svgPainter.drawPolygon(polygon);
        break;
    }
    case pkSvgCases::Kind::Arc:
        svgPainter.drawArc(shape.rect, shape.startAngle16, shape.spanAngle16);
        break;
    }
    const std::string svgData = backend.document();  // :254 真实代码：`const std::string svgData = backend.document();`
    return svgData;
    // :255-277 的真实消费路径（判空 → addCompleteElement / 回退栅格）见文件头；本 driver
    // 只复刻那个判空决策，见 main() 的 DRIVER-FALLBACK 段。
}

// 真 Qt 侧参照文档的「同形」判据：本 driver 的后端默认构造 ⇒ 文档无 `width/height/viewBox`。
// 真 Qt 侧对应 `QSvgGenerator` **不** `setSize`/`setViewBox`（探针里就这么做）。
//
// driver 的输入集：**调用点真的会画出来**的形状——取共享用例表里非退化的椭圆 rect
// （{3,2,20,16} / {3.5,2.25,19.75,15.5} / {0,0,31,31}；排除 {5,5,0,0} 点、{2,2,1,28}
// 极扁、{20,18,-16,-14} 负尺寸、{-4,-3,20,16} 出界这几个**只服务于判据④ 对抗覆盖**的项——
// 真实调用点〔手柄椭圆、knife 弧〕不画这些），加上全部多边形与全部弧（它们本就是
// 现成调用点的形状）。理由与非空论证见 task-3-report.md §3。
bool isCallSiteShapedEllipse(const pkSvgCases::Case &c)
{
    if (c.kind != pkSvgCases::Kind::Ellipse) return true;
    const double w = c.rect.width(), h = c.rect.height();
    return c.rect.x() >= 0 && c.rect.y() >= 0 && w >= 4 && h >= 4;
}

} // namespace

int main()
{
    std::cout << "DRIVER-NOTICE 本可执行体不是真实测试文件：它逐行复刻 libs/flake/svg/"
                 "SvgWriter.cpp:251 附近（SvgWriter::saveGeneric）的代码形状\n";
    std::cout << "DRIVER-NOTICE 复刻对象：libs/flake/svg/SvgWriter.cpp:242-254；真实 SVG 消费路径"
                 "（:255-277，addCompleteElement / 回退栅格）见本文件头注释\n";
    std::cout << "DRIVER-NOTICE 原因：该文件编在 target kritaflake 里，其 CMake 生成产物"
                 "（kritaflake_export.h、ECM 宏、kritaimage/kritaui 闭包）不在 R-54 的 locks 内\n";
    std::cout << "DRIVER-NOTICE 本 driver 是 Qt-free 的，**算不出像素摘要**——下面 DRIVER-DOC-HASH "
                 "只是 Pk 侧文档的 FNV-1a，不等于「与 Qt 一致」；等价性由真 Qt 探针给（见报告 §3）\n";

    // ═══ 复刻段：逐用例跑 call-site 形状，产出文档 ═══════════════════════════════════
    int emitted = 0;
    for (const auto &c : pkSvgCases::table()) {
        if (!isCallSiteShapedEllipse(c)) continue;
        const std::string doc = drvSaveGenericShape(c);
        ++emitted;
        std::cout << c.name << '\t' << doc << '\n';
        std::cout << "DRIVER-DOC-HASH " << c.name << " empty=" << (doc.empty() ? 1 : 0)
                  << " fnv=" << hex32(fnv1a(doc)) << '\n';
        // 结构不变量（复刻代码自身，不是「与 Qt 一致」）：只有「画了东西」的用例才可能有非空文档。
        if (c.hasPen) {
            check(!doc.empty(), c.name + " pen 非 NoPen ⇒ 文档非空");
        }
    }

    // ═══ DRIVER-FALLBACK：复刻 :255 的 `if (svgData.empty())` 两条分支 ═══════════════
    // 让后端进入 `m_supported = false`（不支持的 brush 类型）⇒ document() 返回空 ⇒ 真实代码
    // 走**回退栅格**那条分支（:256-271）。证明判空决策在复刻形状里可达、且返回的确实是空串。
    {
        PkGradient conical(PkGradient::ConicalGradient);   // 非 Linear/Radial ⇒ 后端判不支持
        conical.setColorAt(0.0, PkColor(Pk::red));
        conical.setColorAt(1.0, PkColor(Pk::blue));
        pkSvgCases::Case unsupported;
        unsupported.name = "callsite/fallback/conical-gradient-brush";
        unsupported.kind = pkSvgCases::Kind::Ellipse;
        unsupported.rect = SvgCaseRect(3.0, 2.0, 20.0, 16.0);
        unsupported.hasPen = true;
        unsupported.penWidth = 2.5;
        unsupported.hasBrush = true;      // brush 会被下面显式换成 gradient
        PkSvgPainterBackend backend;
        PkPainter svgPainter(backend);
        svgPainter.setRenderHint(PkPainter::RenderHint::Antialiasing, true);
        svgPainter.setPen(PkPen(PkColor(Pk::black), 2.5));
        svgPainter.setBrush(PkBrush(conical));
        svgPainter.drawEllipse(unsupported.rect);
        const std::string svgData = backend.document();
        std::cout << "DRIVER-FALLBACK " << unsupported.name << " svgData.empty()="
                  << (svgData.empty() ? "true" : "false") << '\n';
        check(svgData.empty(),
              "不支持的 brush（ConicalGradient）⇒ document() 为空 ⇒ 真实代码走回退栅格分支");
    }

    std::cout << "DRIVER-SUMMARY emitted=" << emitted << " failures=" << g_failures << '\n';
    return g_failures == 0 ? 0 : 1;
}
