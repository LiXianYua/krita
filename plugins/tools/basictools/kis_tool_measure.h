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
#include <kis_icon.h>

#include <QVector2D>

class QPointF;
class QWidget;
class QVector2D;

class KoCanvasBase;


class KisToolMeasure : public KisTool
{

    Q_OBJECT

public:
    KisToolMeasure(KoCanvasBase * canvas);
    ~KisToolMeasure() override;

    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;
    void showDistanceAngleOnCanvas();

    QPointF lockedAngle(QPointF pos);

    void paint(QPainter& gc, const KoViewConverter &converter) override;

private:
    QRectF boundingRect();
    double angle();
    double distance();

private:
    QPointF m_startPos {QPointF(0, 0)};
    QPointF m_endPos {QPointF(0, 0)}; 
    QVector2D m_baseLineVec {QPointF(1, 0)};
    bool m_chooseBaseLineVec {false};
};


class KisToolMeasureFactory : public KoToolFactoryBase
{

public:

    KisToolMeasureFactory()
            : KoToolFactoryBase("KritaShape/KisToolMeasure") {
        setSection(ToolBoxSection::View);
        setToolTip(i18n("Measure Tool"));
        setIconName(koIconNameCStr("krita_tool_measure"));
        setPriority(1);
        setActivationShapeId(KRITA_TOOL_ACTIVATION_ID);
    }

    ~KisToolMeasureFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolMeasure(canvas);
    }

};




#endif //KIS_TOOL_MEASURE_H_
