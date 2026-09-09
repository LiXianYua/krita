/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "CalligraphyTool/KarbonCalligraphicShape.h"
#include "CalligraphyTool/KarbonSimplifyPath.h"
#include "KarbonToolsPlugin.h"
#include "KarbonToolsResources.h"

#include <KoPathPoint.h>
#include <KoPathShape.h>
#include <KoShapeRegistry.h>
#include <KoToolRegistry.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QImage>

#include <atomic>
#include <cmath>
#include <cstring>
#include <thread>
#include <type_traits>
#include <vector>

static_assert(std::is_nothrow_invocable_v<KarbonToolsResourceRegistrar,
                                          const KarbonToolsResource &>,
              "resource registrar callbacks must have an enforced no-throw contract");
using PotentiallyThrowingResourceRegistrar = void (*)(const KarbonToolsResource &);
static_assert(!std::is_convertible_v<PotentiallyThrowingResourceRegistrar,
                                     KarbonToolsResourceRegistrar>,
              "a potentially-throwing callback must not satisfy the registrar contract");

namespace
{
bool closeEnough(double lhs, double rhs, double tolerance = 1e-12)
{
    return std::abs(lhs - rhs) < tolerance;
}

bool pointEquals(const PkPointF &point, double x, double y)
{
    return closeEnough(point.x(), x) && closeEnough(point.y(), y);
}

int calligraphicPointsPreserveLiteralGeometry()
{
    KarbonCalligraphicPoint point(PkPointF(10.0, 20.0), 0.0, 4.0);
    if (!pointEquals(point.point(), 10.0, 20.0)) return 10;
    if (!closeEnough(point.angle(), 0.0) || !closeEnough(point.width(), 4.0)) return 11;

    KarbonCalligraphicShape shape;
    shape.appendPointToPath(point);
    if (shape.pointCount() != 2) return 12;
    if (!pointEquals(shape.pointByIndex(KoPathPointIndex(0, 0))->point(), 8.0, 20.0)) return 13;
    if (!pointEquals(shape.pointByIndex(KoPathPointIndex(0, 1))->point(), 12.0, 20.0)) return 14;

    shape.appendPointToPath(KarbonCalligraphicPoint(PkPointF(20.0, 30.0), 0.0, 4.0));
    if (shape.pointCount() != 4) return 15;
    const PkPointF expected[] = {
        PkPointF(8.0, 20.0), PkPointF(18.0, 30.0),
        PkPointF(22.0, 30.0), PkPointF(12.0, 20.0)
    };
    for (int i = 0; i < 4; ++i) {
        if (shape.pointByIndex(KoPathPointIndex(0, i))->point() != expected[i]) return 16 + i;
    }
    return 0;
}

int simplifyRemovesDuplicateWithoutChangingEndpoints()
{
    KoPathShape path;
    path.moveTo(PkPointF(0.0, 0.0));
    path.lineTo(PkPointF(0.0, 0.0));
    path.lineTo(PkPointF(10.0, 0.0));
    karbonSimplifyPath(&path, 0.3);

    if (path.pointCount() != 2) return 30;
    if (!pointEquals(path.pointByIndex(KoPathPointIndex(0, 0))->point(), 0.0, 0.0)) return 31;
    if (!pointEquals(path.pointByIndex(KoPathPointIndex(0, 1))->point(), 10.0, 0.0)) return 32;
    return 0;
}

int simplifyCurvedPathPreservesFittedGeometry()
{
    KoPathShape path;
    path.moveTo(PkPointF(0.0, 0.0));
    path.curveTo(PkPointF(0.0, 12.0), PkPointF(10.0, 12.0), PkPointF(10.0, 0.0));
    path.curveTo(PkPointF(10.0, -12.0), PkPointF(20.0, -12.0), PkPointF(20.0, 0.0));
    karbonSimplifyPath(&path, 0.3);

    if (path.pointCount() != 6 || path.isClosedSubpath(0)) return 33;
    const KoPathPoint *first = path.pointByIndex(KoPathPointIndex(0, 0));
    const KoPathPoint *second = path.pointByIndex(KoPathPointIndex(0, 1));
    const KoPathPoint *middle = path.pointByIndex(KoPathPointIndex(0, 3));
    const KoPathPoint *last = path.pointByIndex(KoPathPointIndex(0, 5));
    if (!pointEquals(first->point(), 0.0, 0.0) || !pointEquals(last->point(), 20.0, 0.0)) return 34;
    if (!pointEquals(second->point(), 1.9281005859375, 7.27734375)) return 35;
    if (!pointEquals(middle->point(), 11.5625, -6.75)) return 36;
    if (!first->activeControlPoint2() || !middle->activeControlPoint1() || !last->activeControlPoint1()) return 37;
    if (!closeEnough(first->controlPoint2().x(), 0.016246701079696606, 1e-9) ||
        !closeEnough(first->controlPoint2().y(), 2.4889605809046409, 1e-9) ||
        !closeEnough(middle->controlPoint1().x(), 8.9474617078288645, 1e-9) ||
        !closeEnough(middle->controlPoint1().y(), -2.5366795628235703, 1e-9) ||
        !closeEnough(last->controlPoint1().x(), 19.909235198516509, 1e-9) ||
        !closeEnough(last->controlPoint1().y(), -1.7047997496029628, 1e-9)) return 38;
    return 0;
}

int calligraphyPreservesSmoothingCapsAndFinalSimplification()
{
    KarbonCalligraphicShape shape(0.5);
    shape.appendPoint(PkPointF(0.0, 0.0), 0.20, 4.0);
    shape.appendPoint(PkPointF(10.0, 4.0), 0.25, 5.0);
    shape.appendPoint(PkPointF(20.0, 9.0), 0.30, 6.0);
    shape.appendPoint(PkPointF(30.0, 13.0), 0.35, 7.0);
    shape.appendPoint(PkPointF(40.0, 10.0), 0.40, 8.0);
    shape.appendPoint(PkPointF(50.0, 5.0), 0.45, 9.0);

    if (shape.pointCount() != 14 || !shape.isClosedSubpath(0)) return 50;
    const KoPathPoint *smoothed = shape.pointByIndex(KoPathPointIndex(0, 3));
    if (!smoothed->activeControlPoint1() || !smoothed->activeControlPoint2()) return 51;
    if (!closeEnough(smoothed->controlPoint1().x(), 16.56518735263418, 1e-9) ||
        !closeEnough(smoothed->controlPoint2().y(), 10.487599893932121, 1e-9)) return 52;

    shape.simplifyGuidePath();
    shape.simplifyPath();
    if (shape.pointCount() != 12 || !shape.isClosedSubpath(0)) return 53;
    const KoPathPoint *first = shape.pointByIndex(KoPathPointIndex(0, 0));
    const KoPathPoint *cap = shape.pointByIndex(KoPathPointIndex(0, 6));
    const KoPathPoint *last = shape.pointByIndex(KoPathPointIndex(0, 11));
    if (!pointEquals(first->point(), 0.95040940539945784, 0.31756405019685519) ||
        !pointEquals(cap->point(), 56.810443015518928, 4.1003177457156461) ||
        !pointEquals(last->point(), 4.6642660817140644, 1.7985745403763587)) return 54;
    if (!cap->activeControlPoint1() || !cap->activeControlPoint2() ||
        !closeEnough(cap->controlPoint1().x(), 54.303193590426531, 1e-9) ||
        !closeEnough(cap->controlPoint2().y(), 5.2019895182987836, 1e-9) ||
        !closeEnough(last->controlPoint1().x(), 11.884579238367628, 1e-9)) return 55;
    return 0;
}

int calligraphyGuideSimplificationRemovesRedundantSections()
{
    KarbonCalligraphicShape shape;
    for (int i = 0; i < 6; ++i) {
        shape.appendPoint(PkPointF(i * 10.0, 0.0), 1.57079635, 4.0);
    }
    if (shape.pointCount() != 14 || !shape.isClosedSubpath(0)) return 56;
    shape.simplifyGuidePath();
    if (shape.pointCount() != 6 || shape.isClosedSubpath(0)) return 57;
    if (!pointEquals(shape.pointByIndex(KoPathPointIndex(0, 1))->point(),
                     10.000000092820414, 0.0) ||
        !pointEquals(shape.pointByIndex(KoPathPointIndex(0, 2))->point(),
                     50.000000092820407, 0.0) ||
        !closeEnough(shape.pointByIndex(KoPathPointIndex(0, 1))->controlPoint2().x(),
                     24.000000092820411, 1e-9)) return 58;
    return 0;
}

KarbonToolsResource capturedResource {};
std::atomic<int> capturedResourceCount {0};
std::atomic<int> replacementResourceCount {0};
std::atomic<int> reentrantResourceCount {0};

void captureResource(const KarbonToolsResource &resource) noexcept
{
    capturedResource = resource;
    ++capturedResourceCount;
}

void captureReplacementResource(const KarbonToolsResource &) noexcept
{
    ++replacementResourceCount;
}

void captureResourceAndReenterRegistration(const KarbonToolsResource &) noexcept
{
    ++reentrantResourceCount;
    registerKarbonToolsResources();
    registerKarbonTools();
}

int nativeResourcePreservesCalligraphyIcon()
{
    if (setKarbonToolsResourceRegistrar(nullptr)) return 40;
    if (!setKarbonToolsResourceRegistrar(captureResource)) return 41;
    if (setKarbonToolsResourceRegistrar(captureReplacementResource)) return 42;

    KoToolRegistry *const toolRegistry = KoToolRegistry::instance();
    KoShapeRegistry *const shapeRegistry = KoShapeRegistry::instance();
    if (toolRegistry->contains(PkString("KarbonCalligraphyTool")) ||
        shapeRegistry->contains(PkString(KarbonCalligraphicShapeId))) return 62;
    const int toolDuplicateCount = toolRegistry->doubleEntries().size();
    const int shapeDuplicateCount = shapeRegistry->doubleEntries().size();

    std::vector<std::thread> callers;
    for (int i = 0; i < 8; ++i) {
        callers.emplace_back(registerKarbonTools);
    }
    for (std::thread &caller : callers) {
        caller.join();
    }
    registerKarbonTools();
    registerKarbonTools();

    if (capturedResourceCount.load() != 1 || replacementResourceCount.load() != 0) return 43;
    if (!toolRegistry->contains(PkString("KarbonCalligraphyTool")) ||
        !shapeRegistry->contains(PkString(KarbonCalligraphicShapeId))) return 63;
    if (toolRegistry->doubleEntries().size() != toolDuplicateCount ||
        shapeRegistry->doubleEntries().size() != shapeDuplicateCount) return 64;
    if (setKarbonToolsResourceRegistrar(captureReplacementResource)) return 44;
    if (setKarbonToolsResourceRegistrar(nullptr)) return 45;

    // The callback receives a temporary descriptor.  A copied descriptor remains
    // usable because the identity strings and payload bytes have static lifetime.
    const KarbonToolsResource icon = capturedResource;
    if (!icon.data || icon.size == 0) return 46;
    if (!icon.iconName || std::strcmp(icon.iconName, "calligraphy") != 0 ||
        !icon.legacyPath || std::strcmp(icon.legacyPath, ":/calligraphy.png") != 0) return 47;

    const QByteArray bytes(reinterpret_cast<const char *>(icon.data), int(icon.size));
    if (QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex() !=
        QByteArray("f3125eea25ad77c9bdf1a783ae7038f006d3c493d86a145e067315f43f7eea12")) return 48;

    const QImage image = QImage::fromData(bytes, "PNG");
    if (image.isNull() || image.width() != 22 || image.height() != 22) return 49;
    if (icon.size < 29 || bytes.mid(12, 4) != QByteArray("IHDR", 4)) return 60;
    if (icon.data[24] != 8 || icon.data[25] != 6 || icon.data[28] != 0) return 61;
    return 0;
}

int lateInstallAfterSkippedRegistrationIsRejected()
{
    if (setKarbonToolsResourceRegistrar(nullptr)) return 70;
    registerKarbonToolsResources();
    if (setKarbonToolsResourceRegistrar(captureResource)) return 71;
    registerKarbonToolsResources();
    if (capturedResourceCount.load() != 0) return 72;
    return 0;
}

int resourceRegistrationAllowsSynchronousReentry()
{
    if (!setKarbonToolsResourceRegistrar(captureResourceAndReenterRegistration)) return 73;

    KoToolRegistry *const toolRegistry = KoToolRegistry::instance();
    KoShapeRegistry *const shapeRegistry = KoShapeRegistry::instance();
    if (toolRegistry->contains(PkString("KarbonCalligraphyTool")) ||
        shapeRegistry->contains(PkString(KarbonCalligraphicShapeId))) return 74;
    const int toolDuplicateCount = toolRegistry->doubleEntries().size();
    const int shapeDuplicateCount = shapeRegistry->doubleEntries().size();

    registerKarbonToolsResources();
    registerKarbonToolsResources();
    registerKarbonTools();

    if (reentrantResourceCount.load() != 1) return 75;
    if (!toolRegistry->contains(PkString("KarbonCalligraphyTool")) ||
        !shapeRegistry->contains(PkString(KarbonCalligraphicShapeId))) return 76;
    if (toolRegistry->doubleEntries().size() != toolDuplicateCount ||
        shapeRegistry->doubleEntries().size() != shapeDuplicateCount) return 77;
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--late-install-after-skip") == 0) {
        return lateInstallAfterSkippedRegistrationIsRejected();
    }
    if (argc == 2 && std::strcmp(argv[1], "--resource-reentry") == 0) {
        return resourceRegistrationAllowsSynchronousReentry();
    }
    const int pointResult = calligraphicPointsPreserveLiteralGeometry();
    if (pointResult) return pointResult;
    const int simplifyResult = simplifyRemovesDuplicateWithoutChangingEndpoints();
    if (simplifyResult) return simplifyResult;
    const int fittedResult = simplifyCurvedPathPreservesFittedGeometry();
    if (fittedResult) return fittedResult;
    const int calligraphyResult = calligraphyPreservesSmoothingCapsAndFinalSimplification();
    if (calligraphyResult) return calligraphyResult;
    const int guideResult = calligraphyGuideSimplificationRemovesRedundantSections();
    if (guideResult) return guideResult;
    return nativeResourcePreservesCalligraphyIcon();
}
