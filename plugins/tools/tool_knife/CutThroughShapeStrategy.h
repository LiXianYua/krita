/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CUT_THROUGH_SHAPE_STRATEGY_H_
#define CUT_THROUGH_SHAPE_STRATEGY_H_

#include <Qt>
#include <QtMath>
#include <PkScopedPointer.h>
#include <PkRect.h>
#include <PkPainter.h>

#include <KoInteractionStrategy.h>
#include <KoShape.h>
#include "GutterWidthsConfig.h"

#include "GutterWidthsConfig.h"

class KoSelection;



class CutThroughShapeStrategy : public KoInteractionStrategy
{
public:
    CutThroughShapeStrategy(KoToolBase *tool, KoSelection *selection, const PkList<KoShape *> &allShapes, PkPointF startPoint, const GutterWidthsConfig &width);

    ~CutThroughShapeStrategy() override;


    KUndo2Command *createCommand() override;

    void handleMouseMove(const PkPointF &mouseLocation, Qt::KeyboardModifiers modifiers) override;
    void finishInteraction(Qt::KeyboardModifiers modifiers) override;
    void paint(PkPainter &painter, const KoViewConverter &converter) override;

private:

    qreal gutterWidthInDocumentCoordinates(qreal lineAngle);
    qreal calculateLineAngle(PkPointF start, PkPointF end);

    static bool willShapeBeCutGeneral(KoShape* referenceShape, const PkPainterPath &srcOutline, bool checkGapLineRect, const PkRectF &gapLineRect);
    static bool willShapeBeCutPrecise(const PkPainterPath& srcOutline, const PkLineF gapLine, const PkLineF& leftLine, const PkLineF& rightLine, const PkPolygonF& gapLinePolygon);

    static void initializeOutlineObjects(const PkTransform &booleanWorkaroundTransform, PkList<KoShape *> allShapes, PkList<PkPainterPath> &outSrcOutlines, PkRectF &outOutlineRect);
    static void initializeGapShapes(PkRectF outlineRect, PkLineF leftLine, PkLineF rightLine, PkPainterPath& outLeft, PkPainterPath& outRight, PkRectF& outGapLineRect, PkPolygonF& outGapLinePolygon);



private:
    friend class CutThroughShapeStrategyTest;

    PkPointF m_startPoint = PkPointF();
    PkPointF m_endPoint = PkPointF();
    PkRectF m_previousLineDirtyRect = PkRectF();
    PkList<KoShape *> m_selectedShapes;
    PkList<KoShape *> m_allShapes;
    GutterWidthsConfig m_width;
};




#endif // CUT_THROUGH_SHAPE_STRATEGY_H_
