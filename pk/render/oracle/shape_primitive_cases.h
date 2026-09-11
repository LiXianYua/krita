// 两侧共用：同一份输入集喂真 Qt 与 Pk。改这里等于改对拍的输入域。
//
// 覆盖原则（R线-spec「对拍怎么做·输入集」）：量级、符号、边界、退化值各取，
// 不只取「典型值」——典型值对拍抓不到边界行为差异。
//
// 本头被两个 TU 各编一次：
//   · oracle/shape_primitive_oracle.cpp   -DPK_SHAPE_QT_ORACLE → 真 QPainter
//   · tests/test_shape_primitive.cpp      （不加宏）        → PkPainter + PkImageRasterBackend
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

struct Case {
    std::string name;
    bool polygon;                   // false = 椭圆，true = 多边形
    CaseRect ellipseRect;           // polygon == false 时用
    std::vector<CasePoint> points;  // polygon == true 时用
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
                c.polygon = false;
                c.ellipseRect = CaseRect(r[0], r[1], r[2], r[3]);
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
                c.polygon = true;
                c.points = pts;
                c.hasPen = pen != 0;
                c.penWidth = 2.5;
                c.hasBrush = brush != 0;
                c.fillRule = 0;
                cases.push_back(c);
            }
        }
    }

    return cases;
}

} // namespace pkShapeCases
