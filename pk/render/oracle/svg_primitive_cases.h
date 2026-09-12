// 两侧共用：同一份输入集喂真 Qt（QSvgGenerator）与 Pk（PkSvgPainterBackend）。
// 改这里等于改对拍的输入域。
//
// 与同目录 shape_primitive_cases.h 的关键形态差别（R-54 §1.3 实测，不是设计选择）：
//   真 Qt 的 QSvgGenerator 对 drawEllipse 产出的是 `<ellipse cx cy rx ry>`、对
//   drawPolygon 显式回起点闭合、对 drawArc 以 `M` 起手只出三次曲线——**两份 SVG 文档的
//   文本按定义就不同**（元素名、数字格式、属性集合/顺序全不同）。所以批 2 的判据④
//   不能做文本 diff：比较面是**同一个渲染器**（真 Qt QSvgRenderer）把两侧各自的文档
//   渲染成同尺寸 ARGB32 图后逐像素比。本仓同一个后端已有先例：
//   libs/flake/flake/tests/PkSvgPainterBackendTest.cpp:63-71。
//
//   因此本头**只共用「输入集」**（Case 的定义与 table()），**不共用渲染**：Pk 侧文档由
//   PkSvgPainterBackend 产出（见文件尾 documentFor，Pk 分支专属），Qt 侧参照文档由
//   oracle/svg_primitive_qt.cpp 用 QSvgGenerator 产出。两侧各自渲染同一个 Case 的同一组
//   参数——这正是对拍要交叉验证的东西。
//
// 本头被三个 TU 各编一次：
//   · oracle/svg_primitive_qt.cpp     -DPK_SVG_QT_ORACLE → 真 QSvgGenerator/QSvgRenderer
//   · oracle/svg_primitive_oracle.cpp （不加宏）        → PkPainter + PkSvgPainterBackend
//   · tests/test_svg_primitive.cpp    （不加宏）        → PkPainter + PkSvgPainterBackend
//
// ⚠ 已知覆盖缺口（与 shape_primitive_cases.h 同源，R-51 评审发现）：
//   drawPolygon 没有 fillRule 形参（PkPainter::drawPolygon(const PkPolygonF&) 单实参，
//   PkDrawPolygonCommand 也无该字段），保留范围内 13 处调用点全部单实参。所以 winding
//   分支从 Pk 侧根本表达不出来，本表也**不取** winding 用例。缺口登记在
//   pk/render/README.md。
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef PK_SVG_QT_ORACLE
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
using SvgCaseRect = QRectF;
using SvgCasePoint = QPointF;
#else
#include "PkPaintCommand.h"
#include "PkPainter.h"
#include "PkSvgPainterBackend.h"
using SvgCaseRect = PkRectF;
using SvgCasePoint = PkPointF;
#endif

namespace pkSvgCases {

enum class Kind { Ellipse, Polygon, Arc };

struct Case {
    std::string name;
    Kind kind;
    SvgCaseRect rect;               // Ellipse / Arc 的边界矩形
    std::vector<SvgCasePoint> points;  // Polygon 时用
    int startAngle16 = 0;           // Arc 起点角（1/16 度，与 PkDrawArcCommand 同）
    int spanAngle16 = 0;            // Arc 跨度角（1/16 度）
    bool hasPen = false;
    double penWidth = 2.5;
    bool hasBrush = false;
};

// 画布：32×32 ARGB32，原点在 (0,0)。两侧都按这个 viewBox 建文档。
// Pk 侧 = PkSvgPainterBackend(PkRectF(0,0,32,32))；Qt 侧 = setSize(32,32)+setViewBox(0,0,32,32)。
inline SvgCaseRect canvasRect()
{
    return SvgCaseRect(0, 0, 32, 32);
}

inline std::vector<Case> table()
{
    std::vector<Case> cases;

    // 与 shape_primitive_cases.h 共用同一份 rect 表：量级、符号、边界、退化值各取。
    //   {20,18,-16,-14} 负尺寸：椭圆/多边形实测 Qt 不 normalize（addEllipse 亦然）；
    //    弧对**两侧**都先 normalize（R-54 §4b 订正），所以它同时也是「归一化后等价」用例。
    const double rects[][4] = {
        {3.0, 2.0, 20.0, 16.0},      // 整数对齐
        {3.5, 2.25, 19.75, 15.5},    // 非整数
        {0.0, 0.0, 31.0, 31.0},      // 贴边、撑满
        {-4.0, -3.0, 20.0, 16.0},    // 部分出界
        {20.0, 18.0, -16.0, -14.0},  // 负尺寸
        {5.0, 5.0, 0.0, 0.0},        // 退化为点
        {2.0, 2.0, 1.0, 28.0},       // 极扁
    };
    int index = 0;

    for (const auto &r : rects) {
        for (int pen = 0; pen < 2; ++pen) {
            for (int brush = 0; brush < 2; ++brush) {
                if (!pen && !brush) continue;  // 两者皆无 = 空操作，无信息
                Case c;
                c.name = "svg-ellipse#" + std::to_string(index++) +
                         (pen ? ":pen2.5" : ":nopen") + (brush ? ":fill" : ":nofill");
                c.kind = Kind::Ellipse;
                c.rect = SvgCaseRect(r[0], r[1], r[2], r[3]);
                c.hasPen = pen != 0;
                c.penWidth = 2.5;
                c.hasBrush = brush != 0;
                cases.push_back(c);
            }
        }
    }

    const std::vector<std::vector<SvgCasePoint>> polys = {
        {SvgCasePoint(3, 3), SvgCasePoint(28, 8), SvgCasePoint(20, 28), SvgCasePoint(6, 22)},  // 凸四边形
        {SvgCasePoint(4, 4)},                                                        // 退化：单点
        {SvgCasePoint(4, 4), SvgCasePoint(20, 20)},                                  // 退化：两点
        {SvgCasePoint(3, 3), SvgCasePoint(28, 28), SvgCasePoint(28, 3), SvgCasePoint(3, 28)},  // 自交
        {SvgCasePoint(2, 2), SvgCasePoint(30, 2), SvgCasePoint(30, 30), SvgCasePoint(2, 30),
         SvgCasePoint(2, 2)},  // 首尾重合（显式闭合）
    };
    for (const auto &pts : polys) {
        for (int pen = 0; pen < 2; ++pen) {
            for (int brush = 0; brush < 2; ++brush) {
                if (!pen && !brush) continue;
                Case c;
                c.name = "svg-polygon#" + std::to_string(index++) +
                         (pen ? ":pen2.5" : ":nopen") + (brush ? ":fill" : ":nofill");
                c.kind = Kind::Polygon;
                c.points = pts;
                c.hasPen = pen != 0;
                c.penWidth = 2.5;
                c.hasBrush = brush != 0;
                cases.push_back(c);
            }
        }
    }

    // 弧族：与椭圆/多边形共用同一份 rect 表，另配起止角覆盖：整圆 / 半圆 / 0 跨度 /
    // 负跨度 / 跨 360 / 起止角落在象限边界。弧**只描边、忽略 brush**（Qt 探针实测
    // fill="none"，R-54 §1.3），所以 hasBrush 两种都取——带 brush 的弧必须与不带的一样
    // （正反用例）；pen==0 而 brush==1 的弧两侧都不画，如实计入退化/空操作。
    struct ArcAngles { int start16; int span16; };
    const ArcAngles arcAngles[] = {
        {0, 180 * 16},        // 半圆
        {0, 360 * 16},        // 整圆（跨 360）
        {0, 0},               // 0 跨度（退化：只剩 arcMoveTo）
        {90 * 16, 90 * 16},   // 起点落在象限边界
        {45 * 16, -60 * 16},  // 负跨度
        {0, -360 * 16},       // 负整圆
        {-30 * 16, 400 * 16}, // 负起点、跨 360
        {270 * 16, 180 * 16}, // 起点在象限边界、跨 360
    };
    for (const auto &r : rects) {
        for (const auto &a : arcAngles) {
            for (int pen = 0; pen < 2; ++pen) {
                for (int brush = 0; brush < 2; ++brush) {
                    if (!pen && !brush) continue;  // 弧两者皆无 = 空操作（brush 又被忽略）
                    Case c;
                    c.name = "svg-arc#" + std::to_string(index++) +
                             ":s" + std::to_string(a.start16) +
                             ":w" + std::to_string(a.span16) +
                             (pen ? ":pen2.5" : ":nopen") + (brush ? ":fill" : ":nofill");
                    c.kind = Kind::Arc;
                    c.rect = SvgCaseRect(r[0], r[1], r[2], r[3]);
                    c.startAngle16 = a.start16;
                    c.spanAngle16 = a.span16;
                    c.hasPen = pen != 0;
                    c.penWidth = 2.5;
                    c.hasBrush = brush != 0;
                    cases.push_back(c);
                }
            }
        }
    }

    return cases;
}

#ifndef PK_SVG_QT_ORACLE
// Pk 侧文档生成：**唯一实现**，oracle 与 ctest 共用（抄第二份 = 两侧悄悄漂移）。
// 每条 Case 只发一条对应命令，其余走 PkPainter 的默认状态——与真实调用点
// （libs/flake/svg/SvgWriter.cpp:251 附近的 `PkSvgPainterBackend backend; PkPainter painter(backend);`）
// 同形。
inline std::string documentFor(const Case &c)
{
    PkSvgPainterBackend backend(canvasRect());
    PkPainter painter(backend);
    painter.setRenderHint(PkPainter::RenderHint::Antialiasing, true);
    painter.setPen(c.hasPen ? PkPen(PkColor(Pk::black), c.penWidth) : PkPen(Pk::NoPen));
    painter.setBrush(c.hasBrush ? PkBrush(PkColor(Pk::red)) : PkBrush(Pk::NoBrush));
    switch (c.kind) {
    case Kind::Ellipse:
        painter.drawEllipse(c.rect);
        break;
    case Kind::Polygon: {
        PkPolygonF polygon;
        for (const auto &p : c.points) polygon.append(p);
        painter.drawPolygon(polygon);
        break;
    }
    case Kind::Arc:
        painter.drawArc(c.rect, c.startAngle16, c.spanAngle16);
        break;
    }
    return backend.document();
}
#endif

} // namespace pkSvgCases
