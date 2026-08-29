/*
 *  kis_tool_polygon.h - part of Krita
 *
 *  SPDX-FileCopyrightText: 2004 Michael Thaler <michael Thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_POLYGON_H_
#define KIS_TOOL_POLYGON_H_

#include "kis_tool_shape.h"
#include "flake/kis_node_shape.h"
#include <kis_tool_polyline_base.h>

class KoCanvasBase;

class KisToolPolygon : public KisToolPolylineBase
{
public:
    KisToolPolygon(KoCanvasBase *canvas);
    ~KisToolPolygon() override;

    bool supportsPaintingAssistants() const override;

protected:
    void finishPolyline(const PkVector<PkPointF>& points) override;
protected:
    void resetCursorStyle() override;
};


#include "KoToolFactoryBase.h"

class KisToolPolygonFactory : public KisToolPolyLineFactoryBase
{

public:
    KisToolPolygonFactory()
            : KisToolPolyLineFactoryBase("KisToolPolygon") {
        setToolTip(PkString("Polygon Tool: Shift-mouseclick ends the polygon."));
        setSection(ToolBoxSection::Shape);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
        setPriority(4);
    }

    ~KisToolPolygonFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolPolygon(canvas);
    }
};


#endif //__KIS_TOOL_POLYGON_H__
