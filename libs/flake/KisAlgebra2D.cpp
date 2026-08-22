/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// 官方 kis_algebra_2d.cpp 的 DecomposedMatrix 构造（QTransform 版），
// 见 KisAlgebra2D.h 文件头注释。与官方差异：去掉断言前的 ENTER_FUNCTION/
// ppVar 调试转储（仅在断言即将失败时执行，功能等价），断言保留。

#include "KisAlgebra2D.h"

#include <kis_assert.h>

bool KisAlgebra2D::fuzzyMatrixCompare(const QTransform &t1, const QTransform &t2, qreal delta)
{
    return
            qAbs(t1.m11() - t2.m11()) < delta &&
            qAbs(t1.m12() - t2.m12()) < delta &&
            qAbs(t1.m13() - t2.m13()) < delta &&
            qAbs(t1.m21() - t2.m21()) < delta &&
            qAbs(t1.m22() - t2.m22()) < delta &&
            qAbs(t1.m23() - t2.m23()) < delta &&
            qAbs(t1.m31() - t2.m31()) < delta &&
            qAbs(t1.m32() - t2.m32()) < delta &&
            qAbs(t1.m33() - t2.m33()) < delta;
}

/********************************************************/
/*             DecomposedMatrix                         */
/********************************************************/

KisAlgebra2D::DecomposedMatrix::DecomposedMatrix()
{
}

KisAlgebra2D::DecomposedMatrix::DecomposedMatrix(const QTransform &t0)
{
    QTransform t(t0);

    QTransform projMatrix;

    if (t.m33() == 0.0 || t0.determinant() == 0.0) {
        qWarning() << "Cannot decompose matrix!" << t;
        valid = false;
        return;
    }

    if (t.type() == QTransform::TxProject) {
        QTransform affineTransform(
            t.m11(), t.m12(), 0,
            t.m21(), t.m22(), 0,
            t.m31(), t.m32(), 1
        );
        projMatrix = affineTransform.inverted() * t;

        t = affineTransform;
        proj[0] = projMatrix.m13();
        proj[1] = projMatrix.m23();
        proj[2] = projMatrix.m33();
    }

    // can't use QVector3D, because they have too little accuracy for ellipse in perspective calculations
    std::array<Eigen::Vector3d, 3> rows;

    //  << t.m11() << t.m12() << t.m13())
    rows[0] = Eigen::Vector3d(t.m11(), t.m12(), t.m13());
    rows[1] = Eigen::Vector3d(t.m21(), t.m22(), t.m23());
    rows[2] = Eigen::Vector3d(t.m31(), t.m32(), t.m33());

    if (!qFuzzyCompare(t.m33(), 1.0)) {
        const qreal invM33 = 1.0 / t.m33();

        for (auto &row : rows) {
            row *= invM33;
        }
    }

    dx = rows[2].x();
    dy = rows[2].y();

    rows[2] = Eigen::Vector3d(0,0,1);

    scaleX = rows[0].norm();
    rows[0] *= 1.0 / scaleX;

    shearXY = rows[0].dot(rows[1]);
    rows[1] = rows[1] - shearXY * rows[0];

    scaleY = rows[1].norm();
    rows[1] *= 1.0 / scaleY;
    shearXY *= 1.0 / scaleY;

    // If determinant is negative, one axis was flipped.
    qreal determinant = rows[0].x() * rows[1].y() - rows[0].y() * rows[1].x();
    if (determinant < 0) {
        // Flip axis with minimum unit vector dot product.
        if (rows[0].x() < rows[1].y()) {
            scaleX = -scaleX;
            rows[0] = -rows[0];
        } else {
            scaleY = -scaleY;
            rows[1] = -rows[1];
        }
        shearXY = - shearXY;
    }

    angle = kisRadiansToDegrees(std::atan2(rows[0].y(), rows[0].x()));

    if (angle != 0.0) {
        // Rotate(-angle) = [cos(angle), sin(angle), -sin(angle), cos(angle)]
        //                = [row0x, -row0y, row0y, row0x]
        // Thanks to the normalization above.

        qreal sn = -rows[0].y();
        qreal cs = rows[0].x();
        qreal m11 = rows[0].x();
        qreal m12 = rows[0].y();
        qreal m21 = rows[1].x();
        qreal m22 = rows[1].y();
        rows[0][0] = (cs * m11 + sn * m21);
        rows[0][1] = (cs * m12 + sn * m22);
        rows[1][0] = (-sn * m11 + cs * m21);
        rows[1][1] = (-sn * m12 + cs * m22);
    }

    QTransform leftOver(
                rows[0].x(), rows[0].y(), rows[0].z(),
            rows[1].x(), rows[1].y(), rows[1].z(),
            rows[2].x(), rows[2].y(), rows[2].z());

    KIS_SAFE_ASSERT_RECOVER_NOOP(fuzzyMatrixCompare(leftOver, QTransform(), 1e-4));
    KIS_ASSERT(fuzzyMatrixCompare(leftOver, QTransform(), 1e-4));
}
