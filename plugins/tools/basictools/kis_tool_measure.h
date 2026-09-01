/*
 *
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_MEASURE_H_
#define KIS_TOOL_MEASURE_H_

#include "kis_tool.h"
#include "kis_global.h"
#include "kis_types.h"
#include "KoToolFactoryBase.h"
#include "flake/kis_node_shape.h"

#include <PkPainter.h>
#include <PkString.h>
#include <PkVectorND.h>

class PkPointF;

class KoCanvasBase;


class KisToolMeasure : public KisTool
{

public:
    KisToolMeasure(KoCanvasBase * canvas);
    ~KisToolMeasure() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void showDistanceAngleOnCanvas();

    PkPointF lockedAngle(PkPointF pos);

    void paint(PkPainter& gc, const KoViewConverter &converter) override;

private:
    PkRectF boundingRect();
    double angle();
    double distance();

private:
    PkPointF m_startPos {PkPointF(0, 0)};
    PkPointF m_endPos {PkPointF(0, 0)};
    PkVector2D m_baseLineVec {PkPointF(1, 0)};
    bool m_chooseBaseLineVec {false};
};


class KisToolMeasureFactory : public KoToolFactoryBase
{

public:

    KisToolMeasureFactory()
            : KoToolFactoryBase("KritaShape/KisToolMeasure") {
        setSection(ToolBoxSection::View);
        setToolTip(PkString("Measure Tool"));
        setPriority(1);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~KisToolMeasureFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolMeasure(canvas);
    }

};




#endif //KIS_TOOL_MEASURE_H_
