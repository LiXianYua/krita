/*
 * This file is part of Krita
 *
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_PERSPECTIVE_MATH_H_
#define _KIS_PERSPECTIVE_MATH_H_

#include "kis_vec.h"
#include <Eigen/Geometry>

typedef Eigen::Matrix<qreal, 3, 3> Matrix3qreal;
typedef Eigen::Matrix<qreal, 9, 9> Matrix9qreal;
typedef Eigen::Matrix<qreal, 9, 1> Vector9qreal;
typedef Eigen::Hyperplane<qreal, 2> LineEquation;

#include <kritaimage_export.h>

class PkRect;

class KRITAIMAGE_EXPORT KisPerspectiveMath
{
private:
    KisPerspectiveMath() { }
public:
    static Matrix3qreal computeMatrixTransfo(const PkPointF& topLeft1, const PkPointF& topRight1, const PkPointF& bottomLeft1, const PkPointF& bottomRight1, const PkPointF& topLeft2, const PkPointF& topRight2, const PkPointF& bottomLeft2, const PkPointF& bottomRight2);
    static Matrix3qreal computeMatrixTransfoToPerspective(const PkPointF& topLeft, const PkPointF& topRight, const PkPointF& bottomLeft, const PkPointF& bottomRight, const PkRect& r);
    static Matrix3qreal computeMatrixTransfoFromPerspective(const PkRect& r, const PkPointF& topLeft, const PkPointF& topRight, const PkPointF& bottomLeft, const PkPointF& bottomRight);
    /// TODO: get rid of this in 2.0
    static inline PkPointF matProd(const Matrix3qreal& m, const PkPointF& p) {
        qreal s = qreal(1) / (p.x() * m.coeff(2, 0) + p.y() * m.coeff(2, 1) + 1.0);
        return PkPointF((p.x() * m.coeff(0, 0) + p.y() * m.coeff(0, 1) + m.coeff(0, 2)) * s,
                       (p.x() * m.coeff(1, 0) + p.y() * m.coeff(1, 1) + m.coeff(1, 2)) * s);
    }
};

#endif
