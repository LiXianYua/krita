/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISALGEBRA2D_QT_SUBSET_H
#define KISALGEBRA2D_QT_SUBSET_H

// S-08 Task 6：上层绘图过渡层的 Qt 版 KisAlgebra2D 子集。
//
// libs/global 的 kis_algebra_2d.h 已按 S-02-a Pk 化（DecomposedMatrix 以
// PkTransform 为构造参数），而上层绘图闭包（S-02-b 交接，随本任务搬入
// libs/flake）保留过渡 Qt（QPainter/QTransform）。此处提供 Qt 兼容的
// DecomposedMatrix 与 leftUnitNormal，仅覆盖 KisHandlePainterHelper 需要的
// 子集，类型还原自官方 v6.0.3 的 kis_algebra_2d.h/.cpp。
//
// 与 libs/global/kis_algebra_2d.h 同名不同内容：本头只被绘图闭包引用，
// 锁内无任何 TU 同时包含两者（codegraph 实测），命名空间不冲突。

#include "kritaflake_export.h"

#include <kis_assert.h>

#include <QTransform>
#include <QPointF>
#include <QDebug>

#include <array>
#include <cmath>

#include <Eigen/Dense>

// 官方 kis_global.h 的全局工具 kisDistance 是 PkPointF 版，此处补 QPointF 版
// 供绘图闭包使用（见文件头注释）。不定义全局 pow2 —— libs/global/kis_global.h
// 已提供同名同签名模板，重定义会与它撞（实测 KoPencilTool/KisHandlePainterHelper
// 两个 TU 压出）；距离计算直接平方内联。
inline qreal kisDistance(const QPointF &pt1, const QPointF &pt2)
{
    const qreal dx = pt1.x() - pt2.x();
    const qreal dy = pt1.y() - pt2.y();
    return std::sqrt(dx * dx + dy * dy);
}

namespace KisAlgebra2D {

template <typename T>
inline T pow2(const T &x)
{
    return x * x;
}

template <typename T>
inline T kisRadiansToDegrees(T radians)
{
    // 官方 kis_global.h 用 M_PI；此处用字面量避免 M_PI 在严格 C++17 下未定义。
    return radians * 180.0 / 3.14159265358979323846264338327950288;
}

template <class T>
inline qreal dotProduct(const T &a, const T &b)
{
    return a.x() * b.x() + a.y() * b.y();
}

template <class T>
inline qreal crossProduct(const T &a, const T &b)
{
    return a.x() * b.y() - a.y() * b.x();
}

template <class T>
inline qreal norm(const T &a)
{
    return std::sqrt(pow2(a.x()) + pow2(a.y()));
}

template <class T>
T leftUnitNormal(const T &a)
{
    T result = a.x() != 0 ? T(-a.y() / a.x(), 1) : T(-1, 0);
    qreal length = norm(result);
    result *= (crossProduct(a, result) >= 0 ? 1 : -1) / length;

    return -result;
}

bool KRITAFLAKE_EXPORT fuzzyMatrixCompare(const QTransform &t1, const QTransform &t2, qreal delta);

struct KRITAFLAKE_EXPORT DecomposedMatrix {
    DecomposedMatrix();

    DecomposedMatrix(const QTransform &t0);

    inline QTransform scaleTransform() const
    {
        return QTransform::fromScale(scaleX, scaleY);
    }

    inline QTransform shearTransform() const
    {
        QTransform t;
        t.shear(shearXY, 0);
        return t;
    }

    inline QTransform rotateTransform() const
    {
        QTransform t;
        t.rotate(angle);
        return t;
    }

    inline QTransform translateTransform() const
    {
        return QTransform::fromTranslate(dx, dy);
    }

    inline QTransform projectTransform() const
    {
        return
            QTransform(
                1,0,proj[0],
                0,1,proj[1],
                0,0,proj[2]);
    }

    inline QTransform transform() const {
        return
            scaleTransform() *
            shearTransform() *
            rotateTransform() *
            translateTransform() *
            projectTransform();
    }

    inline bool isValid() const {
        return valid;
    }

    qreal scaleX = 1.0;
    qreal scaleY = 1.0;
    qreal shearXY = 0.0;
    qreal angle = 0.0;
    qreal dx = 0.0;
    qreal dy = 0.0;
    qreal proj[3] = {0.0, 0.0, 1.0};

private:
    bool valid = true;
};

}

#endif // KISALGEBRA2D_QT_SUBSET_H
