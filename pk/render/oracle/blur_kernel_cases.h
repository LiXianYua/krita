// R-52 对拍工装 · 共用部分：同一个输入集喂真 Qt 与 Pk，两侧各编一份。
//
// 形态照 pk/render/oracle/shape_primitive_cases.h（R-51 交付）：本头被两个 TU
// 各编一次，双分支靠 PK_BLUR_QT_ORACLE 切换，**不是同一个实现换个壳**。
//
//   · oracle/blur_kernel_oracle.cpp   加 -DPK_BLUR_QT_ORACLE → 真 QPainter
//                                     （只由 run_blur_kernel.sh 手工编译，不进任何产物）
//   · tests/test_blur_kernel.cpp      （不加宏）            → PkPainter + PkImageRasterBackend
//
// 复刻对象 = Krita v6.0.3 两个 blur 滤镜的内核构造段：
//   plugins/filters/blur/kis_motion_blur_filter.cpp
//     QImage(kernelSize, Format_RGB32) + fill(0) + QPainter(AA) + setPen(白,1.0)
//     + drawLine(motionLine) → 逐像素 qRed 成卷积核
//   plugins/filters/blur/kis_lens_blur_filter.cpp
//     QImage(kernelW,kernelH, Format_RGB32) + fill(0) + QPainter(AA) + setBrush(白)
//     + setTransform(translate(-boundingRect.x(), -boundingRect.y()))
//     + drawPolygon(transformedIris, Qt::WindingFill) → 逐像素 qRed 成卷积核
// 非内核构造的部分（MotionBlurProperties 的尺寸推导、getIrisPolygon 的形状表）
// 上游与本仓逐字一致，本头按 v6.0.3 原文复刻，lod 固定 0（t.scale 为恒等）。
#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

#ifdef PK_BLUR_QT_ORACLE
#include <QColor>
#include <QImage>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QSize>
#include <QString>
#include <QTransform>
using CaseImage = QImage;
using CaseSize = QSize;
using CasePoint = QPointF;
using CaseLine = QLineF;
using CasePolygon = QPolygonF;
using CaseTransform = QTransform;
using CaseRectF = QRectF;
static constexpr const char *kBackendName = "Qt5Gui";
#else
#include "PkImage.h"
#include "PkImageRasterBackend.h"
#include "PkPainter.h"
#include "PkPaintCommand.h"
using CaseImage = PkImage;
using CaseSize = PkSize;
using CasePoint = PkPointF;
using CaseLine = PkLineF;
using CasePolygon = PkPolygonF;
using CaseTransform = PkTransform;
using CaseRectF = PkRectF;
static constexpr const char *kBackendName = "PkImageRasterBackend";
#endif

// R线-spec「对拍怎么做 · 形态契约」：防 compat 垫片把两侧编成同一个类型，
// 那样对拍恒等、永远绿。两侧的类型根本不在同一个 TU 里，所以「互相不等」写成
// 「本侧确实是本侧那个真类型」——同样是编译期证据，且在这一形态下才是可检的。
#ifdef PK_BLUR_QT_ORACLE
static_assert(std::is_same<CaseImage, QImage>::value,
              "Qt 侧必须用真 QImage（不得被 compat 垫片替换）");
#else
static_assert(std::is_same<CaseImage, PkImage>::value,
              "Pk 侧必须用 PkImage（不得被 compat 垫片替换）");
#endif

namespace pkBlurKernel {

// 内核：行优先的红通道整数矩阵。两侧同一结构——diff 比的是 formatKernelLine 的字符串。
struct Kernel
{
    int width = 0;
    int height = 0;
    std::vector<int> values;
    long long sum = 0;
};

// ── 语料（plan §2.1 P1）──────────────────────────────────────────────────────
// 运动 = 角度 × 长度（7×6=42）；镜头 = 形状 × 半径 × 旋转（6×4×4=96）；共 138 例。
inline const int kMotionAngles[] = {0, 30, 45, 90, 135, 180, 270};
inline const int kMotionLengths[] = {1, 2, 3, 5, 8, 12};
inline const char *const kLensShapes[] = {"Triangle",           "Quadrilateral (4)",
                                          "Pentagon (5)",       "Hexagon (6)",
                                          "Heptagon (7)",       "Octagon (8)"};
inline const unsigned int kLensRadii[] = {1, 2, 5, 9};
inline const unsigned int kLensRotations[] = {0, 17, 45, 90};

// tag 必须由**输入形态参与构造**（R线-spec「规则一」）——不能是每个用例一个字面量。
inline std::string motionTag(int blurAngle, int blurLength)
{
    return "motion_a" + std::to_string(blurAngle) + "_l" + std::to_string(blurLength);
}

inline std::string lensTag(const char *shape, unsigned int irisRadius, unsigned int irisRotation)
{
    // 形状名里的空格换成下划线（"Pentagon (5)" → "Pentagon_(5)"），两侧同一套替换。
    std::string tag = "lens_";
    for (const char *p = shape; *p; ++p) {
        tag += (*p == ' ') ? '_' : *p;
    }
    tag += "_r" + std::to_string(irisRadius) + "_rot" + std::to_string(irisRotation);
    return tag;
}

// 一行 = KERNEL <tag> shape=<W>x<H> sum=<S> values=<行优先，逗号分隔>。
// 两侧打印同一个函数，格式不可能漂。
inline std::string formatKernelLine(const std::string &tag, const Kernel &kernel)
{
    std::string line = "KERNEL " + tag + " shape=" + std::to_string(kernel.width) + "x" +
                       std::to_string(kernel.height) + " sum=" + std::to_string(kernel.sum) +
                       " values=";
    for (std::size_t i = 0; i < kernel.values.size(); ++i) {
        if (i) {
            line += ',';
        }
        line += std::to_string(kernel.values[i]);
    }
    return line;
}

// 逐像素读回红通道。上游写 qRed(image.pixel(i,j))；本仓没有 qRed（R线-spec
//「R-15 遗留缺口」第 3 条），Pk 侧内联 (pixel >> 16) & 0xffu——QImage::pixel /
// PkImage::pixel 都是打包的 0xAARRGGBB（pk/image/PkImage.h 逐字对齐 Qt），等价。
inline Kernel readKernel(const CaseImage &image, int width, int height)
{
    Kernel kernel;
    kernel.width = width;
    kernel.height = height;
    kernel.values.reserve(std::size_t(width) * std::size_t(height));
    for (int j = 0; j < height; ++j) {
        for (int i = 0; i < width; ++i) {
#ifdef PK_BLUR_QT_ORACLE
            const int red = int(qRed(image.pixel(i, j)));
#else
            const int red = int((image.pixel(i, j) >> 16) & 0xffu);
#endif
            kernel.values.push_back(red);
            kernel.sum += red;
        }
    }
    return kernel;
}

// ── 运动模糊：kernel 尺寸推导 + motionLine（上游 MotionBlurProperties，lod=0）────
// kernelHalfSize = ceil(|0.5·len·cosθ|), ceil(|0.5·len·sinθ|)
// kernelSize     = kernelHalfSize*2 + (1,1)
// ⚠ θ=90 时宽度是 3 不是 1（cos(π/2)=6.1e-17，ceil 把 1.5e-16 抬到 1）——plan §2.1 P1。
inline Kernel renderMotionCase(int blurAngle, int blurLength)
{
    const qreal angleRadians = qreal(blurAngle) * M_PI / 180.0;
    const qreal halfWidth = 0.5 * blurLength * std::cos(angleRadians);
    const qreal halfHeight = 0.5 * blurLength * std::sin(angleRadians);

    CaseSize kernelHalfSize;
    kernelHalfSize.rwidth() = int(std::ceil(std::fabs(halfWidth)));
    kernelHalfSize.rheight() = int(std::ceil(std::fabs(halfHeight)));
    const CaseSize kernelSize = kernelHalfSize * 2 + CaseSize(1, 1);

    const CasePoint p1(0.5 * kernelSize.width(), 0.5 * kernelSize.height());
    const CasePoint p2(halfWidth, halfHeight);
    const CaseLine motionLine(p1 - p2, p1 + p2);

    const int width = kernelSize.width();
    const int height = kernelSize.height();

#ifdef PK_BLUR_QT_ORACLE
    // 上游 kis_motion_blur_filter.cpp 的内核构造段逐行。
    CaseImage kernelRepresentation(width, height, QImage::Format_RGB32);
    kernelRepresentation.fill(0);

    QPainter imagePainter(&kernelRepresentation);
    imagePainter.setRenderHint(QPainter::Antialiasing);
    imagePainter.setPen(QPen(QColor::fromRgb(255, 255, 255), 1.0));
    imagePainter.drawLine(motionLine);
    imagePainter.end();  // 上游未显式 end()；读回前收尾，语义无差（plan §2.1 探针同形）
#else
    CaseImage kernelRepresentation(width, height, PkImage::Format_ARGB32);
    // 上游是 QImage(..., QImage::Format_RGB32) + fill(0)；RGB32 的 0 即不透明黑
    // 0xff000000。这里必须显式给不透明黑——实测 fill(0) 会让 138 例里 126 例与真 Qt
    // 不同（plan §2.1 P3 / §2.2 argb32_f0，两侧独立指到同一件事）。
    kernelRepresentation.fill(0xff000000u);

    PkImageRasterBackend backend(kernelRepresentation);
    PkPainter imagePainter(backend);
    imagePainter.setRenderHint(PkPainter::Antialiasing);
    imagePainter.setPen(PkPen(PkColor::fromRgb(255, 255, 255), 1.0));
    imagePainter.drawLine(motionLine);
#endif

    return readKernel(kernelRepresentation, width, height);
}

// ── 镜头模糊：虹膜多边形（上游 getIrisPolygon，lod=0）──────────────────────────
inline int sidesForShape(const char *shape)
{
#ifdef PK_BLUR_QT_ORACLE
    const QString irisShape = QString::fromLatin1(shape);
#else
    const PkString irisShape = PkString::fromLatin1(shape);
#endif
    if (irisShape == "Triangle") return 3;
    if (irisShape == "Quadrilateral (4)") return 4;
    if (irisShape == "Pentagon (5)") return 5;
    if (irisShape == "Hexagon (6)") return 6;
    if (irisShape == "Heptagon (7)") return 7;
    if (irisShape == "Octagon (8)") return 8;
    return 0;  // 上游：未列出的形状 → 空多边形
}

inline CasePolygon irisPolygon(const char *shape, unsigned int irisRadius, unsigned int irisRotation)
{
    if (irisRadius < 1) {
        return CasePolygon();  // 上游 getIrisPolygon 的早退
    }

    const int sides = sidesForShape(shape);
    if (sides == 0) {
        return CasePolygon();
    }

    CasePolygon irisShapePoly;
    qreal angle = 0;
    for (int i = 0; i < sides; ++i) {
        irisShapePoly << CasePoint(0.5 * std::cos(angle), 0.5 * std::sin(angle));
        angle += 2 * M_PI / sides;
    }

    CaseTransform transform;
    transform.rotate(irisRotation);
    transform.scale(irisRadius * 2, irisRadius * 2);

    return transform.map(irisShapePoly);
}

inline Kernel renderLensCase(const char *shape, unsigned int irisRadius, unsigned int irisRotation)
{
    const CasePolygon transformedIris = irisPolygon(shape, irisRadius, irisRotation);
    const CaseRectF boundingRect = transformedIris.boundingRect();

    const int kernelWidth = boundingRect.toAlignedRect().width();
    const int kernelHeight = boundingRect.toAlignedRect().height();

#ifdef PK_BLUR_QT_ORACLE
    // 上游 kis_lens_blur_filter.cpp 的内核构造段逐行。
    CaseImage kernelRepresentation(kernelWidth, kernelHeight, QImage::Format_RGB32);
    kernelRepresentation.fill(0);

    QPainter imagePainter(&kernelRepresentation);
    imagePainter.setRenderHint(QPainter::Antialiasing);
    imagePainter.setBrush(QColor::fromRgb(255, 255, 255));

    QTransform offsetTransform;
    offsetTransform.translate(-boundingRect.x(), -boundingRect.y());
    imagePainter.setTransform(offsetTransform);
    imagePainter.drawPolygon(transformedIris, Qt::WindingFill);
    imagePainter.end();  // 同 renderMotionCase
#else
    CaseImage kernelRepresentation(kernelWidth, kernelHeight, PkImage::Format_ARGB32);
    // 同 renderMotionCase：fill(0xff000000) 是不透明黑，等价上游 RGB32 的 fill(0)。
    kernelRepresentation.fill(0xff000000u);

    PkImageRasterBackend backend(kernelRepresentation);
    PkPainter imagePainter(backend);
    imagePainter.setRenderHint(PkPainter::Antialiasing);
    imagePainter.setBrush(PkColor::fromRgb(255, 255, 255));
    // 上游是 offsetTransform.translate(-boundingRect.x(), -boundingRect.y()) 后
    // setTransform；PkPainter::translate 与 QPainter::translate 同义（都是与当前变换
    // 左乘），从单位阵起步时两者等价（plan §3.2）。
    imagePainter.translate(-boundingRect.x(), -boundingRect.y());
    // 上游带 Qt::WindingFill；PkPainter::drawPolygon 没有 fillRule 形参（`PkDrawPolygon
    // Command` 里也没有这个字段，登记在 pk/render/README.md「drawPolygon cannot express
    // a fill rule」）。**这是有意的**：虹膜多边形是正多边形经仿射变换（rotate + 正
    // scale）所得，恒为凸，实测 6 形状×4 半径×4 旋转 96 例逐像素相同、24 例凸性实测
    // 0 凹（plan §2.1 P2）。
    imagePainter.drawPolygon(transformedIris);
#endif

    return readKernel(kernelRepresentation, kernelWidth, kernelHeight);
}

}  // namespace pkBlurKernel
