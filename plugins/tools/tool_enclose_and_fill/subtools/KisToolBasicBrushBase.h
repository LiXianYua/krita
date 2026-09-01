/*
 *  SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISTOOLBASICBRUSHBASE_H
#define KISTOOLBASICBRUSHBASE_H

#include <kis_tool_shape.h>
#include <PkColor.h>
#include <PkPainter.h>
#include <PkPainterPath.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkSet.h>
#include <PkVector.h>

class KisToolBasicBrushBase : public KisToolShape
{
public:
    enum ToolType {
        PAINT,
        SELECT
    };

    KisToolBasicBrushBase(KoCanvasBase *canvas, ToolType type);
    ~KisToolBasicBrushBase() override;

    void mouseMoveEvent(KoPointerEvent *event) override;
    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void activateAlternateAction(AlternateAction action) override;
    void deactivateAlternateAction(AlternateAction action) override;
    void beginAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void continueAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void endAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void paint(PkPainter& gc, const KoViewConverter &converter) override;

    qreal pressureToCurve(qreal pressure);

public:
    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;
    void setPreviewColor(const PkColor &color);

protected:
    virtual void finishStroke(const PkPainterPath& stroke) = 0;
    KisOptimizedBrushOutline getOutlinePath(const PkPointF &documentPos,
                                const KoPointerEvent *event,
                                KisPaintOpSettings::OutlineMode outlineMode) override;

protected:
    void updateSettings();
    void resetCursorStyle() override;

private:
    static constexpr int levelOfPressureResolution = 1024;
    static constexpr int feedbackLineWidth{2};

    PkPainterPath m_path;
    PkPointF m_lastPosition;
    qreal m_lastPressure {1.0};
    ToolType m_type {PAINT};

    PkVector<qreal> m_pressureSamples;
    OutlineStyle m_outlineStyle {OUTLINE_FULL};
    bool m_showOutlineWhilePainting {true};
    bool m_forceAlwaysFullSizedOutline {true};

    PkPointF m_changeSizeInitialGestureDocPoint;
    PkPointF m_changeSizeLastDocumentPoint;
    qreal m_changeSizeLastPaintOpSize {0.0};
    PkPoint m_changeSizeInitialGestureGlobalPoint;

    PkColor m_previewColor;

    PkPainterPath generateSegment(const PkPointF &point1, qreal radius1, const PkPointF &point2, qreal radius2) const;
    void update(const PkRectF &strokeSegmentRect);
};

#endif
