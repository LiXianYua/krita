/*
    SPDX-FileCopyrightText: 2026 S-03-a
    SPDX-License-Identifier: LGPL-2.1-or-later

    PkGradient —— 渐变色标的零 Qt 垫片（S 线剥 Qt 用，行为对齐 Qt 5.15 语义，
    只剥不改）。单个值类型类，用 Type 字段区分渐变种类，不做多态：Krita 消费方
    （KoStopGradient 的渐变导出/导入、KisGradientConversion）对渐变对象的操作都
    落在 type()/spread()/coordinateMode()/stops()/几何 getter 这一组非虚接口上，
    type 字段即可覆盖。
 */

#ifndef PK_GRADIENT_H
#define PK_GRADIENT_H

#include <PkGlobal.h>     // qreal
#include <PkColor.h>      // PkColor
#include <PkPoint.h>      // PkPointF
#include <PkVector.h>     // PkVector

namespace PkGradientEnums {
enum Type { NoGradient, LinearGradient, RadialGradient, ConicalGradient };
enum Spread { PadSpread, ReflectSpread, RepeatSpread };
enum CoordinateMode { LogicalMode, ObjectBoundingMode };
}

// 渐变 stop：offset + 颜色。消费方按 offset/color 取用。
struct PkGradientStop {
    qreal offset;
    PkColor color;
};
using PkGradientStops = PkVector<PkGradientStop>;

// 短名别名（plan 的 Step 1：struct GradientStop，别名 Stop / Stops）。
using GradientStop = PkGradientStop;
using GradientStops = PkGradientStops;

class PkGradient
{
public:
    PkGradient();                                    // Type=NoGradient
    explicit PkGradient(PkGradientEnums::Type type);

    // 便捷静态构造（对齐 LinearGradient(start, finalStop) /
    // RadialGradient(center, radius, focalPoint) / ConicalGradient(center, angle)）。
    static PkGradient linear(const PkPointF &start, const PkPointF &finalStop);
    static PkGradient radial(const PkPointF &center, qreal radius, const PkPointF &focalPoint);
    static PkGradient conical(const PkPointF &center, qreal angle);

    PkGradientEnums::Type type() const;
    void setType(PkGradientEnums::Type);
    PkGradientEnums::Spread spread() const;
    void setSpread(PkGradientEnums::Spread);
    PkGradientEnums::CoordinateMode coordinateMode() const;
    void setCoordinateMode(PkGradientEnums::CoordinateMode);

    PkGradientStops stops() const;
    void setStops(const PkGradientStops &);
    void setColorAt(qreal pos, const PkColor &color);
    // 线性插值：pos 夹在 [0,1]，按 stops 排序插值；无 stops 返回黑（0,0,0）。
    PkColor colorAt(qreal pos) const;

    // 几何参数（按 type 各取所需；未用到的参数保持默认值）
    PkPointF start() const;
    void setStart(const PkPointF &);
    void setStart(qreal x, qreal y);
    PkPointF finalStop() const;
    void setFinalStop(const PkPointF &);
    void setFinalStop(qreal x, qreal y);
    PkPointF center() const;
    void setCenter(const PkPointF &);
    void setCenter(qreal x, qreal y);
    qreal radius() const;
    void setRadius(qreal);
    PkPointF focalPoint() const;
    void setFocalPoint(const PkPointF &);
    void setFocalPoint(qreal x, qreal y);
    qreal angle() const;
    void setAngle(qreal);

private:
    PkGradientEnums::Type m_type;
    PkGradientEnums::Spread m_spread;
    PkGradientEnums::CoordinateMode m_coordinateMode;
    PkGradientStops m_stops;
    PkPointF m_start;
    PkPointF m_finalStop;
    PkPointF m_center;
    PkPointF m_focalPoint;
    qreal m_radius;
    qreal m_angle;
};

#endif // PK_GRADIENT_H
