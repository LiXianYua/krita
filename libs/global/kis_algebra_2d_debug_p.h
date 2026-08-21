/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_ALGEBRA_2D_DEBUG_P_H
#define KIS_ALGEBRA_2D_DEBUG_P_H

#include "kis_algebra_2d.h"

#include <PkDebug.h>

#include <ostream>

namespace KisAlgebra2D {
namespace Private {

namespace Detail {

template<typename T>
inline void writeDebugField(std::ostream &stream, std::streamsize width, const T &value)
{
    if (width != 0) {
        stream.width(width);
    }
    stream << value;
}

} // namespace Detail

struct VectorPathPointDebugValue
{
    const VectorPath::VectorPathPoint &point;
};

inline std::ostream &operator<<(std::ostream &stream, const VectorPathPointDebugValue &value)
{
    const VectorPath::VectorPathPoint &point = value.point;
    const std::streamsize width = stream.width();
    const auto write = [&stream, width](const auto &field) {
        Detail::writeDebugField(stream, width, field);
    };

    write(point.type == VectorPath::VectorPathPoint::MoveTo
              ? "(move "
              : (point.type == VectorPath::VectorPathPoint::BezierTo ? "(curve " : "(line "));
    write("(");
    write(point.endPoint.x());
    write(", ");
    write(point.endPoint.y());
    write(")");
    if (point.type == VectorPath::VectorPathPoint::BezierTo) {
        write(": (");
        write("(");
        write(point.controlPoint1.x());
        write(", ");
        write(point.controlPoint1.y());
        write(")");
        write(", ");
        write("(");
        write(point.controlPoint2.x());
        write(", ");
        write(point.controlPoint2.y());
        write(")");
        write(")");
    }
    write(")");
    return stream;
}

inline void writeVectorPathPoint(PkDebug debug, const VectorPath::VectorPathPoint &point)
{
    debug << VectorPathPointDebugValue{point};
}

} // namespace Private
} // namespace KisAlgebra2D

#endif // KIS_ALGEBRA_2D_DEBUG_P_H
