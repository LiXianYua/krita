/*
 *  kis_vec.h - part of KImageShop
 *
 *  SPDX-FileCopyrightText: 1999 Matthias Elter <me@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __kis_vec_h__
#define __kis_vec_h__

#include <PkPoint.h>
#include <Eigen/Core>
#include <PkVectorND.h>


typedef Eigen::Matrix<qreal, 2, 1> KisVector2D;

inline KisVector2D toKisVector2D(const PkPointF& p)
{
    return KisVector2D(p.x(), p.y());
}
inline KisVector2D toKisVector2D(const PkPoint& p)
{
    return KisVector2D(p.x(), p.y());
}

template<typename ExpressionType>
inline PkPointF toQPointF(const ExpressionType& expr)
{
    return PkPointF(expr.x(), expr.y());
}

#endif
