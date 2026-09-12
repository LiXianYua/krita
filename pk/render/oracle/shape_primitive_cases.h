// 两侧共用：同一份输入集喂真 Qt 与 Pk。改这里等于改对拍的输入域。
//
// 覆盖原则（R线-spec「对拍怎么做·输入集」）：量级、符号、边界、退化值各取，
// 不只取「典型值」——典型值对拍抓不到边界行为差异。
//
// 本头被两个 TU 各编一次：
//   · oracle/shape_primitive_oracle.cpp   -DPK_SHAPE_QT_ORACLE → 真 QPainter
//   · tests/test_shape_primitive.cpp      （不加宏）        → PkPainter + PkImageRasterBackend
//
// ⚠ 已知覆盖缺口（R-51 评审发现，登记于此）：
//   `Case::fillRule` 永远是 0。**不是因为忘了**——`PkPainter::drawPolygon(const PkPolygonF&)`
//   没有 fillRule 形参，而真 Qt 的 `QPainter::drawPolygon(const QPolygonF&, Qt::FillRule)`
//   有；`PkDrawPolygonCommand` 里也没有这个字段。所以 winding 分支**从 Pk 侧根本表达不出来**。
//   保留范围实测：13 处 drawPolygon 调用点**全部是单实参形式**，0 处用 WindingFill
//   ——按 R线-spec「判据① 撞上零用量时留着并登记，不要删」，本字段留着，缺口记在这里
//   与 pk/render/README.md。
#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#ifdef PK_SHAPE_QT_ORACLE
#include <QBrush>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
using CaseImage = QImage;
using CaseRect = QRectF;
using CasePoint = QPointF;
using CasePolygon = QPolygonF;
inline CaseImage makeCaseImage() { return CaseImage(32, 32, QImage::Format_ARGB32); }
inline void fillCaseImage(CaseImage &image) { image.fill(Qt::white); }
static constexpr const char *kBackendName = "Qt5Gui";
#else
#include "PkImage.h"
#include "PkImageRasterBackend.h"
#include "PkPaintCommand.h"
#include "PkPainter.h"
using CaseImage = PkImage;
using CaseRect = PkRectF;
using CasePoint = PkPointF;
using CasePolygon = PkPolygonF;
// PkImageRasterBackend::fillPath 只接受 Format_ARGB32 / Format_Grayscale8 目的地
// （libs/flake/PkImageRasterBackend.cpp:802），此处固定用 ARGB32——与 Qt 侧同格式。
inline CaseImage makeCaseImage() { return CaseImage(32, 32, PkImage::Format_ARGB32); }
inline void fillCaseImage(CaseImage &image) { image.fill(Pk::white); }
static constexpr const char *kBackendName = "PkImageRasterBackend";
#endif

namespace pkShapeCases {

enum class Kind { Ellipse, Polygon, Arc };

struct Case {
    std::string name;
    Kind kind;
    CaseRect shapeRect;             // Ellipse / Arc 的边界矩形
    std::vector<CasePoint> points;  // Polygon 时用
    int startAngle16 = 0;           // Arc 起点角（1/16 度，与 PkDrawArcCommand 同）
    int spanAngle16 = 0;            // Arc 跨度角（1/16 度）
    bool hasPen;
    double penWidth;
    bool hasBrush;
    int fillRule;                   // 0 = odd-even, 1 = winding
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

// 唯一的渲染实现，两侧共用。**不要在 oracle/*.cpp 或 tests/*.cpp 里再内联一份**——
// 抄第二份的结果是两边悄悄漂移，对拍就变成「自己跟自己比」。
inline CaseImage renderCase(const Case &c)
{
    CaseImage image = makeCaseImage();
    fillCaseImage(image);
#ifdef PK_SHAPE_QT_ORACLE
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(c.hasPen ? QPen(Qt::black, c.penWidth) : QPen(Qt::NoPen));
    painter.setBrush(c.hasBrush ? QBrush(Qt::red) : QBrush(Qt::NoBrush));
    switch (c.kind) {
    case Kind::Polygon: {
        QPolygonF polygon;
        for (const auto &p : c.points) polygon << p;
        painter.drawPolygon(polygon, c.fillRule ? Qt::WindingFill : Qt::OddEvenFill);
        break;
    }
    case Kind::Arc:
        painter.drawArc(c.shapeRect, c.startAngle16, c.spanAngle16);
        break;
    case Kind::Ellipse:
        painter.drawEllipse(c.shapeRect);
        break;
    }
    painter.end();
#else
    PkImageRasterBackend backend(image);
    PkPainter painter(backend);
    painter.setRenderHint(PkPainter::RenderHint::Antialiasing, true);
    painter.setPen(c.hasPen ? PkPen(PkColor(Pk::black), c.penWidth) : PkPen(Pk::NoPen));
    painter.setBrush(c.hasBrush ? PkBrush(PkColor(Pk::red)) : PkBrush(Pk::NoBrush));
    switch (c.kind) {
    case Kind::Polygon: {
        PkPolygonF polygon;
        for (const auto &p : c.points) polygon.append(p);
        painter.drawPolygon(polygon);
        break;
    }
    case Kind::Arc:
        painter.drawArc(c.shapeRect, c.startAngle16, c.spanAngle16);
        break;
    case Kind::Ellipse:
        painter.drawEllipse(c.shapeRect);
        break;
    }
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

inline std::vector<Case> table()
{
    std::vector<Case> cases;

    const double rects[][4] = {
        {3.0, 2.0, 20.0, 16.0},      // 整数对齐
        {3.5, 2.25, 19.75, 15.5},    // 非整数
        {0.0, 0.0, 31.0, 31.0},      // 贴边、撑满
        {-4.0, -3.0, 20.0, 16.0},    // 部分出界
        {20.0, 18.0, -16.0, -14.0},  // 负尺寸（实测 Qt 不 normalize，与 addEllipse 同）
        {5.0, 5.0, 0.0, 0.0},        // 退化为点
        {2.0, 2.0, 1.0, 28.0},       // 极扁
    };
    int index = 0;
    for (const auto &r : rects) {
        for (int pen = 0; pen < 2; ++pen) {
            for (int brush = 0; brush < 2; ++brush) {
                if (!pen && !brush) continue;  // 两者皆无 = 空操作，无信息
                Case c;
                c.name = "ellipse#" + std::to_string(index++) +
                         (pen ? ":pen2.5" : ":nopen") + (brush ? ":fill" : ":nofill");
                c.kind = Kind::Ellipse;
                c.shapeRect = CaseRect(r[0], r[1], r[2], r[3]);
                c.hasPen = pen != 0;
                c.penWidth = 2.5;
                c.hasBrush = brush != 0;
                c.fillRule = 0;
                cases.push_back(c);
            }
        }
    }

    const std::vector<std::vector<CasePoint>> polys = {
        {CasePoint(3, 3), CasePoint(28, 8), CasePoint(20, 28), CasePoint(6, 22)},  // 凸四边形
        {CasePoint(4, 4)},                                                        // 退化：单点
        {CasePoint(4, 4), CasePoint(20, 20)},                                     // 退化：两点
        {CasePoint(3, 3), CasePoint(28, 28), CasePoint(28, 3), CasePoint(3, 28)},  // 自交
        {CasePoint(2, 2), CasePoint(30, 2), CasePoint(30, 30), CasePoint(2, 30),
         CasePoint(2, 2)},  // 首尾重合（显式闭合）
    };
    for (const auto &pts : polys) {
        for (int pen = 0; pen < 2; ++pen) {
            for (int brush = 0; brush < 2; ++brush) {
                if (!pen && !brush) continue;
                Case c;
                c.name = "polygon#" + std::to_string(index++) +
                         (pen ? ":pen2.5" : ":nopen") + (brush ? ":fill" : ":nofill");
                c.kind = Kind::Polygon;
                c.points = pts;
                c.hasPen = pen != 0;
                c.penWidth = 2.5;
                c.hasBrush = brush != 0;
                c.fillRule = 0;
                cases.push_back(c);
            }
        }
    }

    // 弧族（R-54 批 1b）：与椭圆/多边形**共用同一份 rect 表**（整数/非整数/负尺寸/
    // 退化矩形这些手挑对抗用例都在里面），另配一组起止角覆盖：整圆 / 半圆 / 0 跨度 /
    // 负跨度 / 跨 360 / 起止角落在象限边界。
    //
    // 本族**复刻的真实调用点形状**（R-54 Task 3 Step 1 复核）：`PkPainter::drawArc`
    // 的活调用点全集 = 2 处 / 1 文件，都在
    // `plugins/tools/tool_knife/CutThroughShapeStrategy.cpp:371-372`
    // （`CutThroughShapeStrategy::paint`，`:336` 起）。两行的形参形状都是
    //   painter.drawArc(<PkRectF>, int startAngle16, int spanAngle16)
    // —— 同一个 `drawArc(const PkRectF &, int, int)`、同一个 `PkPainter &painter`
    // 接收者。起止角由 `-qtAngleFactor*kisRadiansToDegrees(directionBetweenPoints(...))`
    // 算出（qreal → int 截断），所以真实调用点**一般传非 16 倍数的角**（这正是
    // Task 3 Step 1b 要在下表补非 16 倍数角度的原因）。`paint()` 在 `:341` 只 `setPen`
    // （灰 2px）、从不 `setBrush` ⇒ 真实形态落在 hasBrush=false 一侧；本族两种都取。
    // 弧**只描边、忽略 brush**（Qt 与 Pk 一致，见 libs/flake/PkImageRasterBackend.cpp
    // 的弧分支）：所以 hasBrush 两种都取，带 brush 的弧必须与不带的一样（正反用例）；
    // 注意 rect 表里的负尺寸项对**弧**与对椭圆行为不同：Qt 的 drawArc 先把 rect 归一化
    // （实测见 R-54 Task 1 probe），Pk 弧分支照做；所以负尺寸 rect 同时也是一组
    // 「归一化后等价」的对抗输入。
    // pen==0 而 brush==1 的弧两侧都不画（brush 被忽略），如实计入退化/空操作。
    struct ArcAngles { int start16; int span16; };
    // 既有 8 组（R-54 批 1b）：起止角**全部是 16 的倍数**。
    const ArcAngles arcAngles[] = {
        {0, 180 * 16},        // 半圆
        {0, 360 * 16},        // 整圆（跨 360）
        {0, 0},               // 0 跨度（退化：只剩 moveTo）
        {90 * 16, 90 * 16},   // 起点落在象限边界
        {45 * 16, -60 * 16},  // 负跨度
        {0, -360 * 16},       // 负整圆
        {-30 * 16, 400 * 16}, // 负起点、跨 360
        {270 * 16, 180 * 16}, // 起点在象限边界、跨 360
    };
    // R-54 Task 3 Step 1b：**非** 16 倍数的角（`x % 16 != 0`）。上面 8 组全是 16 的倍数
    // ⇒ 后端那两行换算 `arc->startAngle16 / 16.0` 与 `arc->spanAngle16 / 16.0` 在旧用例表上
    // **没有任何判别力**（改成整数除法 `/16` 是编译期可证的空操作——R线-spec〈注入「抓不到」
    // 时，先怀疑注入〉点名的那一类）。真实调用点（CutThroughShapeStrategy.cpp:371-372）的角
    // 由 `-16*kisRadiansToDegrees(...)`（qreal → int 截断）算出，**一般不是 16 的倍数**。
    // 这 4 组覆盖：起点正小数 / 起点负小数 / 跨象限边界 / 只让 span 带小数。
    // 非空论证：每组至少一个字段满足 `x % 16 != 0`，故 `x/16.0 != x/16` 必然成立
    //   {1000, 2880}:  1000%16=8  ⇒ 起点 62.5°（正小数），span 整 16 倍
    //   {-1003,-960}:  -1003%16=-11 ⇒ 起点 -62.6875°（负小数），span 整 16 倍
    //   {719, 900}:    719%16=15, 900%16=4 ⇒ 起点 44.9375°、跨度 56.25° ⇒ 终点 101.1875° 跨 90° 象限边界
    //   {0, 1001}:     1001%16=9  ⇒ 起点 0°（整），跨度 62.5625°（**只让 span 带小数**）
    const ArcAngles arcAnglesFrac16[] = {
        {1000, 180 * 16},
        {-1003, -60 * 16},
        {719, 900},
        {0, 1001},
    };
    auto appendArc = [&](const double *r, const ArcAngles &a) {
        for (int pen = 0; pen < 2; ++pen) {
            for (int brush = 0; brush < 2; ++brush) {
                if (!pen && !brush) continue;  // 弧两者皆无 = 空操作（brush 又被忽略）
                Case c;
                c.name = "arc#" + std::to_string(index++) +
                         ":s" + std::to_string(a.start16) +
                         ":w" + std::to_string(a.span16) +
                         (pen ? ":pen2.5" : ":nopen") + (brush ? ":fill" : ":nofill");
                c.kind = Kind::Arc;
                c.shapeRect = CaseRect(r[0], r[1], r[2], r[3]);
                c.startAngle16 = a.start16;
                c.spanAngle16 = a.span16;
                c.hasPen = pen != 0;
                c.penWidth = 2.5;
                c.hasBrush = brush != 0;
                c.fillRule = 0;
                cases.push_back(c);
            }
        }
    };
    // 既有 8 组先出（index 与用例名**逐字节不变**），非 16 倍数的 4 组**追加在末尾**——
    // 若把它们插进既有循环，`index` 会整体后移、既有用例的名字会变，违反「只增不改」。
    for (const auto &r : rects) for (const auto &a : arcAngles) appendArc(r, a);
    for (const auto &r : rects) for (const auto &a : arcAnglesFrac16) appendArc(r, a);

    return cases;
}

} // namespace pkShapeCases
