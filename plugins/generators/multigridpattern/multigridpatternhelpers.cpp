/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "multigridpatternhelpers.h"

#include <PkLine.h>
#include <cmath>

namespace {

PkList<int> indicesFromPoint(PkPointF point,
                             const PkList<qreal> &angles,
                             qreal offset)
{
    PkList<int> indices;
    for (int i = 0; i < angles.size(); ++i) {
        const qreal index = point.x() * std::sin(angles.at(i)) +
                            point.y() * std::cos(angles.at(i));
        indices.append(std::floor(index - offset + 1));
    }
    return indices;
}

PkPointF projectedVertex(const PkList<int> &indices,
                         const PkList<qreal> &angles)
{
    if (indices.isEmpty() || angles.isEmpty()) {
        return PkPointF();
    }

    qreal x = 0;
    qreal y = 0;
    for (int i = 0; i < indices.size(); ++i) {
        x += indices.at(i) * std::cos(angles.at(i));
        y += indices.at(i) * std::sin(angles.at(i));
    }
    return PkPointF(x, y);
}

}

PkString multigridDefaultGradientXml()
{
    // Keep the legacy QDomDocument byte representation as a persistence
    // contract. PkXmlDocument uses a different attribute order/empty-element
    // spelling, so serialization here must remain an explicit literal.
    return PkString(
        "<gradient type=\"stop\">\n"
        " <stop alpha=\"1\" bitdepth=\"U8\" offset=\"0\" stoptype=\"0\">\n"
        "  <RGB r=\"0\" g=\"1\" b=\"0\" space=\"sRGB-elle-V2-srgbtrc.icc\"/>\n"
        " </stop>\n"
        " <stop alpha=\"1\" bitdepth=\"U8\" offset=\"1\" stoptype=\"0\">\n"
        "  <RGB r=\"0\" g=\"0\" b=\"1\" space=\"sRGB-elle-V2-srgbtrc.icc\"/>\n"
        " </stop>\n"
        "</gradient>\n");
}

PkList<KisMultiGridRhomb> generateMultigridRhombs(int lines,
                                                   int divisions,
                                                   qreal offset)
{
    PkList<KisMultiGridRhomb> rhombs;
    PkList<qreal> angles;

    const int halfLines = divisions;
    const int totalLines = halfLines * 2 + 1;

    for (int i = 0; i < lines; ++i) {
        angles.append(2 * (M_PI / lines) * i);
    }

    for (int i = 0; i < angles.size(); ++i) {
        const qreal angle1 = angles.at(i);
        const PkPointF p1(totalLines * std::cos(angle1),
                          -totalLines * std::sin(angle1));
        const PkPointF p2 = -p1;

        for (int parallel1 = 0; parallel1 < totalLines; ++parallel1) {
            const int index1 = halfLines - parallel1;
            const PkPointF offset1((index1 + offset) * std::sin(angle1),
                                   (index1 + offset) * std::cos(angle1));
            PkLineF line1(p1, p2);
            line1.translate(offset1);

            for (int k = i + 1; k < angles.size(); ++k) {
                const qreal angle2 = angles.at(k);
                const PkPointF p3(totalLines * std::cos(angle2),
                                  -totalLines * std::sin(angle2));
                const PkPointF p4 = -p3;

                for (int parallel2 = 0; parallel2 < totalLines; ++parallel2) {
                    const int index2 = halfLines - parallel2;
                    const PkPointF offset2((index2 + offset) * std::sin(angle2),
                                           (index2 + offset) * std::cos(angle2));
                    PkLineF line2(p3, p4);
                    line2.translate(offset2);

                    PkPointF intersection;
                    if (line1.intersects(line2, &intersection) !=
                        PkLineF::BoundedIntersection) {
                        continue;
                    }

                    PkList<int> indices =
                        indicesFromPoint(intersection, angles, offset);
                    PkPolygonF shape;
                    indices[i] = index1 + 1;
                    indices[k] = index2 + 1;
                    shape << projectedVertex(indices, angles);
                    indices[i] = index1;
                    shape << projectedVertex(indices, angles);
                    indices[k] = index2;
                    shape << projectedVertex(indices, angles);
                    indices[i] = index1 + 1;
                    shape << projectedVertex(indices, angles);
                    indices[k] = index2 + 1;
                    shape << projectedVertex(indices, angles);

                    KisMultiGridRhomb rhomb;
                    rhomb.shape = shape;
                    rhomb.parallel1 = index1;
                    rhomb.parallel2 = index2;
                    rhomb.line1 = i;
                    rhomb.line2 = k;
                    rhombs.append(rhomb);
                }
            }
        }
    }

    return rhombs;
}
