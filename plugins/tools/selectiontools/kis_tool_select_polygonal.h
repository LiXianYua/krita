/*
 *  kis_tool_select_polygonal.h - part of Krayon^WKrita
 *
 *  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@compuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_SELECT_POLYGONAL_H_
#define KIS_TOOL_SELECT_POLYGONAL_H_

#include <PkPainter.h>
#include <PkPainterPath.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkSet.h>
#include <PkVector.h>

#include "KisSelectionToolFactoryBase.h"
#include "kis_tool_polyline_base.h"
#include <kis_tool_select_base.h>
#include "kis_selection_tool_config_widget_helper.h"

class __KisToolSelectPolygonalLocal : public KisToolPolylineBase
{
public:
    __KisToolSelectPolygonalLocal(KoCanvasBase *canvas);

    void beginPrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void beginPrimaryDoubleClickAction(KoPointerEvent *event) override;
    void beginAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void mouseMoveEvent(KoPointerEvent *event) override;
    void paint(PkPainter &painter, const KoViewConverter &converter) override;
    void activate(const PkSet<KoShape *> &shapes) override;
    void deactivate() override;
    void requestStrokeEnd() override;
    void requestStrokeCancellation() override;
    KisPopupWidgetInterface *popupWidget() override;

    void undoSelectionOrCancel();

protected:
    virtual void finishPolyline(const PkVector<PkPointF> &points) = 0;
    virtual void beginShape() {}
    virtual void endShape() {}

private:
    void undoSelection();
    void endStroke();
    void cancelStroke();
    void updateArea();
    PkRectF dragBoundingRect();

    PkPointF m_dragStart;
    PkPointF m_dragEnd;
    bool m_dragging {false};
    PkVector<PkPointF> m_points;
    bool m_closeSnappingActivated {false};
};

class KisToolSelectPolygonal : public KisToolSelectBase<__KisToolSelectPolygonalLocal>
{
public:
    KisToolSelectPolygonal(KoCanvasBase* canvas);
    void resetCursorStyle() override;
    void undoSelectionOrCancel()
    { __KisToolSelectPolygonalLocal::undoSelectionOrCancel(); }
private:
    void finishPolyline(const PkVector<PkPointF> &points) override;
    void beginShape() override;
    void endShape() override;
};



class KisToolSelectPolygonalFactory : public KisSelectionToolFactoryBase
{
public:
    KisToolSelectPolygonalFactory()
        : KisSelectionToolFactoryBase("KisToolSelectPolygonal")
    {
        setToolTip(PkString("Polygonal Selection Tool"));
        setSection(ToolBoxSection::Select);
        setPriority(2);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~KisToolSelectPolygonalFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolSelectPolygonal(canvas);
    }

};

#endif //__selecttoolpolygonal_h__
