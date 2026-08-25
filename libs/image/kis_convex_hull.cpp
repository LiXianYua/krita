#include "kis_convex_hull.h"

#include "kis_paint_device.h"
#include "kis_random_accessor_ng.h"
#include "KoColorSpace.h"
#include "KoColor.h"
#include "KoColorModelStandardIds.h"

#include <PkElapsedTimer.h>
#include <boost/geometry.hpp>


namespace boost
{
    namespace geometry
    {
        namespace traits
        {
            // Adapt PkPoint to Boost.Geometry

            template<> struct tag<PkPoint>
            { typedef point_tag type; };

            template<> struct coordinate_type<PkPoint>
            { typedef int type; };

            template<> struct coordinate_system<PkPoint>
            { typedef cs::cartesian type; };

            template<> struct dimension<PkPoint> : boost::mpl::int_<2> {};

            template<>
            struct access<PkPoint, 0>
            {
                static int get(PkPoint const& p)
                {
                    return p.x();
                }

                static void set(PkPoint& p, int const& value)
                {
                    p.rx() = value;
                }
            };

            template<>
            struct access<PkPoint, 1>
            {
                static int get(PkPoint const& p)
                {
                    return p.y();
                }

                static void set(PkPoint& p, int const& value)
                {
                    p.ry() = value;
                }
            };

            // Adapt PkPolygon to Boost.Geometry as Linestring

            template<> struct tag<PkPolygon>
            { typedef linestring_tag type; };
            
        }
    }

    template <>
    struct range_iterator<PkPolygon>
    { typedef PkPolygon::iterator type; };

    template<>
    struct range_const_iterator<PkPolygon>
    { typedef PkPolygon::const_iterator type; };
} // namespace boost::geometry::traits

namespace {

PkPolygon convexHull(const PkVector<PkPoint> &points)
{
    PkPolygon hull;
    boost::geometry::convex_hull(PkPolygon(points), hull);
    return hull;
}

// From libs/image/kis_paint_device.cc
struct CheckFullyTransparent {
    CheckFullyTransparent(const KoColorSpace *colorSpace)
        : m_colorSpace(colorSpace)
    {
    }

    bool isPixelEmpty(const quint8 *pixelData)
    {
        return m_colorSpace->opacityU8(pixelData) == OPACITY_TRANSPARENT_U8;
    }

private:
    const KoColorSpace *m_colorSpace;
};

struct CheckNonDefault {
    CheckNonDefault(int pixelSize, const quint8 *defaultPixel)
        : m_pixelSize(pixelSize),
          m_defaultPixel(defaultPixel)
    {
    }

    bool isPixelEmpty(const quint8 *pixelData)
    {
        return memcmp(m_defaultPixel, pixelData, m_pixelSize) == 0;
    }

private:
    int m_pixelSize;
    const quint8 *m_defaultPixel;
};

struct CheckDeselected {
    CheckDeselected(const KoColorSpace *colorSpace)
        : m_colorSpace(colorSpace),
          m_deselectedColor(Qt::black, colorSpace),
          m_pixelSize(colorSpace->pixelSize())
    {
        KIS_SAFE_ASSERT_RECOVER_NOOP(colorSpace->colorModelId() == AlphaColorModelID ||
                                     colorSpace->colorModelId() == GrayAColorModelID);
    }

    bool isPixelEmpty(const quint8 *pixelData)
    {
        return m_colorSpace->opacityU8(pixelData) == OPACITY_TRANSPARENT_U8 ||
            memcmp(m_deselectedColor.data(), pixelData, m_pixelSize) == 0;
    }

private:
    const KoColorSpace *m_colorSpace;
    const KoColor m_deselectedColor;
    const int m_pixelSize;
};

template <class ComparePixelOp>
PkVector<PkPoint> retrieveAllBoundaryPointsImpl(const KisPaintDevice *device, const PkRect &rect, const PkRect &skip, ComparePixelOp compareOp)
{
    PkVector<PkPoint> points;
    int defaultMin = rect.x() + rect.width() + 1;
    int defaultMax = rect.x() - 1;
    PkVector<int> minX(rect.height(), defaultMin);
    PkVector<int> maxX(rect.height(), defaultMax);
    int base = rect.top();
    if (!skip.isEmpty()) {
        for (int y = skip.top(); y <= skip.bottom(); y++) {
            minX[y - base] = skip.left();
            maxX[y - base] = skip.right();
        }
    }

    int pixelSize = device->pixelSize();
    KisRandomConstAccessorSP accessor = device->createRandomConstAccessorNG();
    for (int y = rect.top(); y <= rect.bottom();) {
        int rows = accessor->numContiguousRows(y);
        for (int x = rect.left(); x <= rect.right();) {
            int columns = accessor->numContiguousColumns(x);
            accessor->moveTo(x, y);
            int strideBytes = accessor->rowStride(x, y);
            const quint8 *data = accessor->rawDataConst();
            for (int r = 0; r < rows; r++) {
                for (int c = 0; c < columns; c++) {
                    if (!compareOp.isPixelEmpty(data + c * pixelSize)) {
                        int index = y + r - base;
                        minX[index] = std::min(minX[index], x + c);
                        maxX[index] = std::max(maxX[index], x + c);
                    }
                }
                data += strideBytes;
            }
            x += columns;
        }
        y += rows;
    }

    for (int y = rect.top(); y <= rect.bottom(); y++) {
        int index = y - base;
        if (minX[index] < defaultMin) {
            points.append(PkPoint(minX[index], y));
            points.append(PkPoint(minX[index], y + 1));
        }
        if (maxX[index] > defaultMax) {
            points.append(PkPoint(maxX[index] + 1, y));
            points.append(PkPoint(maxX[index] + 1, y + 1));
        }
    }
    
    return points;
}
// This matches the behavior of KisPaintDevice::calculateExactBounds(false), whose result is returned by KisPaintDevice::exactBounds()
PkVector<PkPoint> retrieveAllBoundaryPoints(const KisPaintDevice *device) {
    PkRect rect = device->extent();

    const KoColor defaultPixel = device->defaultPixel();
    const quint8 defaultOpacity = defaultPixel.opacityU8();

    PkVector<PkPoint> points;

    if (defaultOpacity != OPACITY_TRANSPARENT_U8) {
        PkRect skip = device->defaultBounds()->bounds();
        CheckNonDefault compareOp(device->pixelSize(), defaultPixel.data());

        points = retrieveAllBoundaryPointsImpl(device, rect, skip, compareOp);
        if (!skip.isEmpty()) {
            int x, y, w, h;
            skip.getRect(&x, &y, &w, &h);
            points.append(PkPoint(x, y));
            points.append(PkPoint(x + w, y));
            points.append(PkPoint(x + w, y + h));
            points.append(PkPoint(x, y + h));
        }
    } else {
        CheckFullyTransparent compareOp(device->colorSpace());
        points = retrieveAllBoundaryPointsImpl(device, rect, PkRect(), compareOp);
    }
    return points;
}

PkVector<PkPoint> retrieveAllBoundaryPointsSelectionLike(const KisPaintDevice *device) {
    PkRect rect = device->extent();

    const KoColor defaultPixel = device->defaultPixel();
    const quint8 defaultOpacity = defaultPixel.opacityU8();

    PkVector<PkPoint> points;

    if (defaultOpacity != OPACITY_TRANSPARENT_U8 &&
        defaultPixel != KoColor(Qt::black, defaultPixel.colorSpace())) {

        PkRect skip = device->defaultBounds()->bounds();
        CheckNonDefault compareOp(device->pixelSize(), defaultPixel.data());

        points = retrieveAllBoundaryPointsImpl(device, rect, skip, compareOp);
        if (!skip.isEmpty()) {
            int x, y, w, h;
            skip.getRect(&x, &y, &w, &h);
            points.append(PkPoint(x, y));
            points.append(PkPoint(x + w, y));
            points.append(PkPoint(x + w, y + h));
            points.append(PkPoint(x, y + h));
        }
    } else if (device->colorSpace()->colorModelId() == AlphaColorModelID) {
        CheckFullyTransparent compareOp(device->colorSpace());
        points = retrieveAllBoundaryPointsImpl(device, rect, PkRect(), compareOp);
    } else {
        // pre-condition:
        // defaultOpacity == OPACITY_TRANSPARENT_U8 ||
        // defaultPixel == "deselected"

        CheckDeselected compareOp(device->colorSpace());
        points = retrieveAllBoundaryPointsImpl(device, rect, PkRect(), compareOp);
    }
    return points;
}

}

namespace KisConvexHull {

PkPolygon findConvexHull(const PkVector<PkPoint> &points)
{
    PkPolygon hull = convexHull(points);
    return hull;
}

PkPolygon findConvexHull(KisPaintDeviceSP device)
{
    PkElapsedTimer timer;
    timer.start();
    auto ps = retrieveAllBoundaryPoints(device);
    auto p = findConvexHull(ps);
    return p;
}

PkPolygon findConvexHullSelectionLike(KisPaintDeviceSP device)
{
    PkElapsedTimer timer;
    timer.start();
    auto ps = retrieveAllBoundaryPointsSelectionLike(device);
    auto p = findConvexHull(ps);
    return p;
}

}
