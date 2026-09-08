/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KIS_TOOL_RECTANGLE_BASE_H
#define KIS_TOOL_RECTANGLE_BASE_H

#include <PkSet.h>
#include <kis_tool_shape.h>

class KRITACANVAS_EXPORT KisToolRectangleBase : public KisToolShape
{
signals:

    void rectangleChanged(const PkRectF &newRect);
    void sigRequestReloadConfig();

public:
    void constraintsChanged(bool forceRatio, bool forceWidth, bool forceHeight, float ratio, float width, float height);
    void roundCornersChanged(int rx, int ry);

    void requestStrokeEnd() override;
    void requestStrokeCancellation() override;

public:
    enum ToolType {
        PAINT,
        SELECT
    };

    explicit KisToolRectangleBase(KoCanvasBase * canvas, KisToolRectangleBase::ToolType type, const QCursor & cursor);

    void pkKeyPressEvent(PkToolKeyEvent *event) override;
    void pkKeyReleaseEvent(PkToolKeyEvent *event) override;
    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;

    void paint(PkPainter &gc, const KoViewConverter &converter) override;
    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;

    void showSize();

protected:
    virtual void finishRect(const PkRectF &rect, qreal roundCornersX, qreal roundCornersY) = 0;

    PkPointF m_dragCenter;
    PkPointF m_dragStart;
    PkPointF m_dragEnd;
    ToolType m_type;

    bool m_isRatioForced;
    bool m_isWidthForced;
    bool m_isHeightForced;
    bool m_rotateActive;
    float m_forcedRatio;
    float m_forcedWidth;
    float m_forcedHeight;
    int m_roundCornersX;
    int m_roundCornersY;
    qreal m_referenceAngle;
    qreal m_angle;
    qreal m_angleBuffer;
    Pk::KeyboardModifiers m_currentModifiers;

    bool isFixedSize();
    qreal getRotationAngle();
    PkPainterPath drawX(const PkPointF &pt);
    void applyConstraints(PkSizeF& area, bool overrideRatio);
    void getRotatedPath(PkPainterPath &path, const PkPointF &center, const qreal &angle);

    void updateArea();
    virtual void paintRectangle(PkPainter &gc, const PkRectF &imageRect);
    virtual PkRectF createRect(const PkPointF &start, const PkPointF &end);
    virtual bool showRoundCornersGUI() const;

    void endStroke();
    void cancelStroke();
};

#endif // KIS_TOOL_RECTANGLE_BASE_H
