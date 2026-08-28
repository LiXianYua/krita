/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2007 Rob Buis <buis@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KOSPIRALSHAPE_H
#define KOSPIRALSHAPE_H

#include "KoParameterShape.h"
#include <PkNamespace.h>
#include <PkPoint.h>
#include <PkSize.h>
#include <PkString.h>
#include <PkTransform.h>

#define SpiralShapeId "SpiralShape"

/**
 * This class adds support for the spiral
 * shape.
 */
class SpiralShape : public KoParameterShape
{
public:
    /// the possible spiral types
    enum SpiralType {
        Curve = 0,   ///< spiral uses curves
        Line = 1    ///< spiral uses lines
    };

    SpiralShape();
    ~SpiralShape() override;

    KoShape* cloneShape() const override;

    void setSize(const PkSizeF &newSize);
    PkPointF normalize();

    /**
     * Sets the type of the spiral.
     * @param type the new spiral type
     */
    void setType(SpiralType type);

    /// Returns the actual spiral type
    SpiralType type() const;

    /**
     * Sets the fade parameter of the spiral.
     * @param angle the new start angle in degree
     */
    void setFade(qreal fade);

    /// Returns the actual fade parameter
    qreal fade() const;

    bool clockWise() const;
    void setClockWise(bool clockwise);

    /// reimplemented
    PkString pathShapeId() const;

protected:
    SpiralShape(const SpiralShape &rhs);

    void moveHandleAction(int handleId, const PkPointF &point, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void updatePath(const PkSizeF &size);
    void createPath(const PkSizeF &size);

private:
    void updateKindHandle();
    void updateAngleHandles();

    // fade parameter
    qreal m_fade;
    // angle for modifying the kind in radiant
    qreal m_kindAngle;
    // the center of the spiral
    PkPointF m_center;
    // the radii of the spiral
    PkPointF m_radii;
    // the actual spiral type
    SpiralType m_type;
    //
    bool m_clockwise;

    KoSubpath m_points;
};

#endif /* KOSPIRALSHAPE_H */
