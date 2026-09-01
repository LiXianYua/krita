#include "path_rasterizer_cases.h"

#include "../private/kis_path_rasterizer_p.h"
#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkVector.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <new>

namespace allocation_fault {
bool failNext = false;
bool didFail = false;
}

void *operator new(std::size_t size)
{
    if (allocation_fault::failNext) {
        allocation_fault::failNext = false;
        allocation_fault::didFail = true;
        throw std::bad_alloc();
    }
    if (void *memory = std::malloc(size ? size : 1)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void *memory) noexcept
{
    std::free(memory);
}

void operator delete(void *memory, std::size_t) noexcept
{
    std::free(memory);
}

namespace {

PkPainterPath makePath(const RasterCase &c)
{
    PkPainterPath path;
    path.setFillRule(c.fillRule == 1 ? Qt::WindingFill : Qt::OddEvenFill);
    for (std::size_t i = 0; i < c.commandCount; ++i) {
        const auto &cmd = c.commands[i];
        switch (cmd.verb) {
        case PathVerb::MoveTo: path.moveTo(cmd.x1, cmd.y1); break;
        case PathVerb::LineTo: path.lineTo(cmd.x1, cmd.y1); break;
        case PathVerb::CubicTo:
            path.cubicTo(cmd.x1, cmd.y1, cmd.x2, cmd.y2, cmd.x3, cmd.y3);
            break;
        case PathVerb::Close: path.closeSubpath(); break;
        }
    }
    return path;
}

bool isMode(const RasterCase &c, RasterMode mode)
{
    return c.mode == mode;
}

bool valid(const RasterCase &c)
{
    return c.clipWidth > 0 && c.clipHeight > 0
        && c.clipWidth <= 4096 && c.clipHeight <= 4096
        && c.commands != nullptr && c.commandCount > 0;
}

bool samePattern(const PkVector<qreal> &actual,
                 std::initializer_list<qreal> expected)
{
    if (actual.size() != int(expected.size())) {
        return false;
    }
    int index = 0;
    for (qreal value : expected) {
        if (actual.at(index++) != value) {
            return false;
        }
    }
    return true;
}

bool isCanonicalEmpty(const KisPathRasterizer::CoverageMask &mask)
{
    return mask.bounds.isEmpty() && mask.stride == 0 && mask.alpha.empty();
}

PkPainterPath makeLine(qreal x1, qreal y1, qreal x2, qreal y2)
{
    PkPainterPath path;
    path.moveTo(x1, y1);
    path.lineTo(x2, y2);
    return path;
}

int verifyCosmeticDomainBoundaries()
{
    constexpr int coordinateLimit = 32766;
    constexpr qreal dashOffsetLimit = 32000000.0;
    const PkRect smallClip(0, 0, 16, 16);
    const PkPen cosmetic;

    if (KisPathRasterizer::rasterizeStroke(
            makeLine(-coordinateLimit, 8, 8, 8), cosmetic, smallClip, false).isEmpty()
        || KisPathRasterizer::rasterizeStroke(
            makeLine(8, 8, coordinateLimit, 8), cosmetic, smallClip, false).isEmpty()) {
        return 31;
    }
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            makeLine(-coordinateLimit - 1.0, 8, 8, 8), cosmetic, smallClip, false))
        || !isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            makeLine(8, 8, coordinateLimit + 1.0, 8), cosmetic, smallClip, false))) {
        return 32;
    }

    if (KisPathRasterizer::rasterizeStroke(
            makeLine(1, 0, 2, 0), cosmetic,
            PkRect(0, 0, coordinateLimit, 1), false).isEmpty()
        || KisPathRasterizer::rasterizeStroke(
            makeLine(0, 1, 0, 2), cosmetic,
            PkRect(0, 0, 1, coordinateLimit), false).isEmpty()) {
        return 33;
    }
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            makeLine(1, 0, 2, 0), cosmetic,
            PkRect(0, 0, coordinateLimit + 1, 1), false))
        || !isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            makeLine(0, 1, 0, 2), cosmetic,
            PkRect(0, 0, 1, coordinateLimit + 1), false))) {
        return 34;
    }

    PkVector<qreal> dashPattern;
    dashPattern.append(2.0);
    dashPattern.append(1.0);
    const auto dashMask = [&](qreal offset) {
        PkPen pen;
        pen.setDashPattern(dashPattern);
        pen.setDashOffset(offset);
        return KisPathRasterizer::rasterizeStroke(
            makeLine(1, 8, 15, 8), pen, smallClip, false);
    };
    if (dashMask(dashOffsetLimit).isEmpty()
        || dashMask(-dashOffsetLimit).isEmpty()) {
        return 35;
    }
    if (!isCanonicalEmpty(dashMask(dashOffsetLimit + 1.0))
        || !isCanonicalEmpty(dashMask(-dashOffsetLimit - 1.0))
        || !isCanonicalEmpty(dashMask(std::numeric_limits<qreal>::max()))) {
        return 36;
    }
    return 0;
}

void armAllocationFailure()
{
    allocation_fault::didFail = false;
    allocation_fault::failNext = true;
}

bool consumedAllocationFailure()
{
    allocation_fault::failNext = false;
    return allocation_fault::didFail;
}

int verifyAllocationBoundaries()
{
    const PkRect clip(0, 0, 16, 16);
    const PkPainterPath faultLine = makeLine(1, 1, 8, 8);

    PkPen dashedPen;
    dashedPen.setWidthF(3.5);
    dashedPen.setStyle(Qt::DashLine);
    armAllocationFailure();
    try {
        const auto mask = KisPathRasterizer::rasterizeStroke(
            faultLine, dashedPen, clip, true);
        if (!consumedAllocationFailure() || !isCanonicalEmpty(mask)) {
            return 37;
        }
    } catch (const std::bad_alloc &) {
        consumedAllocationFailure();
        return 37;
    }

    PkPen solidPen;
    solidPen.setWidthF(3.5);
    armAllocationFailure();
    try {
        const auto mask = KisPathRasterizer::rasterizeStroke(
            faultLine, solidPen, clip, true);
        if (!consumedAllocationFailure() || !isCanonicalEmpty(mask)) {
            return 38;
        }
    } catch (const std::bad_alloc &) {
        consumedAllocationFailure();
        return 38;
    }

    return 0;
}

int verifyPenCarrierTransitions()
{
    const PkPen defaults;
    if (defaults.widthF() != 1.0 || defaults.style() != Qt::SolidLine
        || defaults.capStyle() != Qt::SquareCap
        || defaults.joinStyle() != Qt::BevelJoin
        || defaults.miterLimit() != 2.0 || defaults.dashOffset() != 0.0
        || !defaults.dashPattern().isEmpty()) {
        return 20;
    }

    PkVector<qreal> custom;
    custom.append(2.0);
    custom.append(3.0);
    PkPen resetByStyle;
    resetByStyle.setDashPattern(custom);
    resetByStyle.setDashOffset(0.75);
    resetByStyle.setStyle(Qt::DashLine);
    if (resetByStyle.style() != Qt::DashLine
        || resetByStyle.dashOffset() != 0.0
        || !samePattern(resetByStyle.dashPattern(), {4.0, 2.0})) {
        return 21;
    }

    PkPen promoted;
    promoted.setStyle(Qt::DashDotLine);
    promoted.setDashOffset(1.25);
    if (promoted.style() != Qt::CustomDashLine
        || promoted.dashOffset() != 1.25
        || !samePattern(promoted.dashPattern(), {4.0, 2.0, 1.0, 2.0})) {
        return 22;
    }

    PkPen emptyNoOp;
    emptyNoOp.setStyle(Qt::DotLine);
    emptyNoOp.setDashPattern({});
    if (emptyNoOp.style() != Qt::DotLine
        || !samePattern(emptyNoOp.dashPattern(), {1.0, 2.0})) {
        return 23;
    }

    PkVector<qreal> odd;
    odd.append(5.0);
    odd.append(2.0);
    odd.append(1.5);
    PkPen completedOddPattern;
    completedOddPattern.setDashPattern(odd);
    if (completedOddPattern.style() != Qt::CustomDashLine
        || !samePattern(completedOddPattern.dashPattern(), {5.0, 2.0, 1.5, 1.0})) {
        return 24;
    }
    return 0;
}

int emitCase(const RasterCase &c)
{
    if (!valid(c)) {
        return 3;
    }
    const PkRect clip(c.clipX, c.clipY, c.clipWidth, c.clipHeight);
    KisPathRasterizer::CoverageMask mask;
    if (c.mode == RasterMode::Fill) {
        mask = KisPathRasterizer::rasterizeFill(makePath(c), clip, c.antialiased);
    } else {
        PkPen pen(Qt::white);
        pen.setWidthF(c.penWidth);
        pen.setStyle(static_cast<Qt::PenStyle>(c.penStyle));
        pen.setCapStyle(static_cast<Qt::PenCapStyle>(c.capStyle));
        pen.setJoinStyle(static_cast<Qt::PenJoinStyle>(c.joinStyle));
        pen.setMiterLimit(c.miterLimit);
        if (c.dashCount) {
            PkVector<qreal> pattern;
            for (std::size_t i = 0; i < c.dashCount; ++i) {
                pattern.append(c.dashPattern[i]);
            }
            pen.setDashPattern(pattern);
            pen.setDashOffset(c.dashOffset);
        }
        mask = KisPathRasterizer::rasterizeStroke(makePath(c), pen, clip, c.antialiased);
    }
    if (mask.bounds != clip || mask.stride != c.clipWidth
        || mask.alpha.size() != std::size_t(c.clipWidth) * std::size_t(c.clipHeight)) {
        return 3;
    }

    std::printf("KPR1\n%s\n%d %d %d %d\n",
                c.name, c.clipX, c.clipY, c.clipWidth, c.clipHeight);
    for (uint8_t coverage : mask.alpha) {
        std::printf("%02x", coverage);
    }
    std::putchar('\n');
    return 0;
}

int verifyNonFiniteInputsAreRejected()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const PkRect clip(0, 0, 16, 16);

    PkPainterPath line;
    line.moveTo(1.0, 1.0);
    line.lineTo(nan, 8.0);
    line.lineTo(8.0, 8.0);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeFill(line, clip, false))) {
        return 10;
    }

    PkPainterPath cubic;
    cubic.moveTo(1.0, 1.0);
    cubic.cubicTo(2.0, infinity, 7.0, 3.0, 8.0, 8.0);
    cubic.lineTo(1.0, 8.0);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeFill(cubic, clip, true))) {
        return 11;
    }

    PkPainterPath negativeInfinity;
    negativeInfinity.moveTo(-infinity, 1.0);
    negativeInfinity.lineTo(8.0, 1.0);
    negativeInfinity.lineTo(8.0, 8.0);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeFill(negativeInfinity, clip, false))) {
        return 12;
    }

    PkPainterPath finiteLine;
    finiteLine.moveTo(1.0, 1.0);
    finiteLine.lineTo(8.0, 8.0);

    PkPen noPen;
    noPen.setStyle(Qt::NoPen);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, noPen, clip, true))) {
        return 13;
    }

    const PkPainterPath emptyPath;
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            emptyPath, PkPen(), clip, true))) {
        return 14;
    }

    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, PkPen(), PkRect(0, 0, 0, 16), true))) {
        return 15;
    }

    PkPen invalidWidth(Qt::black, nan);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, invalidWidth, clip, true))) {
        return 16;
    }

    PkPen invalidMiter;
    invalidMiter.setMiterLimit(infinity);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, invalidMiter, clip, true))) {
        return 17;
    }

    PkVector<qreal> invalidPattern;
    invalidPattern.append(2.0);
    invalidPattern.append(nan);
    PkPen invalidDash;
    invalidDash.setDashPattern(invalidPattern);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, invalidDash, clip, true))) {
        return 18;
    }

    PkPen invalidStyle;
    invalidStyle.setStyle(static_cast<Qt::PenStyle>(99));
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, invalidStyle, clip, true))) {
        return 25;
    }

    PkPen invalidCap;
    invalidCap.setCapStyle(static_cast<Qt::PenCapStyle>(99));
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, invalidCap, clip, true))) {
        return 26;
    }

    PkPen invalidJoin;
    invalidJoin.setJoinStyle(static_cast<Qt::PenJoinStyle>(99));
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, invalidJoin, clip, true))) {
        return 27;
    }

    PkPen invalidOffset;
    invalidOffset.setDashOffset(infinity);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, invalidOffset, clip, true))) {
        return 28;
    }

    PkPen negativeWidth(Qt::black, -1.0);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, negativeWidth, clip, true))) {
        return 29;
    }

    PkPen negativeMiter;
    negativeMiter.setMiterLimit(-1.0);
    if (!isCanonicalEmpty(KisPathRasterizer::rasterizeStroke(
            finiteLine, negativeMiter, clip, true))) {
        return 30;
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--self-test-invalid") == 0) {
        return verifyNonFiniteInputsAreRejected();
    }
    if (argc == 2 && std::strcmp(argv[1], "--self-test-carrier") == 0) {
        return verifyPenCarrierTransitions();
    }
    if (argc == 2 && std::strcmp(argv[1], "--self-test-boundary") == 0) {
        return verifyCosmeticDomainBoundaries();
    }
    if (argc == 2 && std::strcmp(argv[1], "--self-test-allocation") == 0) {
        return verifyAllocationBoundaries();
    }
    const bool listAll = argc == 2 && std::strcmp(argv[1], "--list") == 0;
    const bool listFill = argc == 2 && std::strcmp(argv[1], "--list-fill") == 0;
    const bool listStroke = argc == 2 && std::strcmp(argv[1], "--list-stroke") == 0;
    if (listAll || listFill || listStroke) {
        for (const auto &c : pathRasterizerCases()) {
            if (listAll || (listFill && isMode(c, RasterMode::Fill))
                || (listStroke && isMode(c, RasterMode::Stroke))) {
                std::printf("%s\n", c.name);
            }
        }
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--case") == 0) {
        for (const auto &c : pathRasterizerCases()) {
            if (std::strcmp(argv[2], c.name) == 0) {
                return emitCase(c);
            }
        }
        return 2;
    }
    return 2;
}
