/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoShapeRubberSelectStrategy.h"
#include "KoShapeRubberSelectStrategy_p.h"
#include "KoViewConverter.h"

#include <pk/render/PkPainter.h>

#include "KoShapeManager.h"
#include "KoSelection.h"
#include "KoCanvasBase.h"
// [migrate] missing include for Pk/Qt type
#include <PkColor.h>
// 原生数值谓词的名字（pk/global/PkGlobal.h）——Qt 名 pkMin/pkMax 只在 qt 桶里存在。
// 形制照 libs/flake/KoMarker.cpp。
#include <PkGlobal.h>

KoShapeRubberSelectStrategy::KoShapeRubberSelectStrategy(KoToolBase *tool, const PkPointF &clicked, bool useSnapToGrid)
    : KoInteractionStrategy(*(new KoShapeRubberSelectStrategyPrivate(tool)))
{
    Q_D(KoShapeRubberSelectStrategy);
    d->snapGuide->enableSnapStrategies(KoSnapGuide::GridSnapping);
    d->snapGuide->enableSnapping(useSnapToGrid);

    d->selectRect = PkRectF(d->snapGuide->snap(clicked, Pk::KeyboardModifiers()), PkSizeF(0, 0));
}

void KoShapeRubberSelectStrategy::paint(PkPainter &painter, const KoViewConverter &converter)
{
    Q_D(KoShapeRubberSelectStrategy);
    painter.setRenderHint(PkPainter::Antialiasing, false);

    const PkColor crossingColor(80,130,8);
    const PkColor coveringColor(8,60,167);

    PkColor selectColor(
        currentMode() == CrossingSelection ?
        crossingColor : coveringColor);

    selectColor.setAlphaF(0.8);
    PkPen select(selectColor, decorationThickness());
    select.setCosmetic(true);
    painter.setPen(select);

    selectColor.setAlphaF(0.4);
    painter.setBrush(PkBrush(selectColor));

    PkRectF paintRect = converter.documentToView(d->selectedRect());
    paintRect = paintRect.normalized();

    painter.drawRect(paintRect);
}

void KoShapeRubberSelectStrategy::handleMouseMove(const PkPointF &p, Pk::KeyboardModifiers modifiers)
{
    Q_D(KoShapeRubberSelectStrategy);
    PkPointF point = d->snapGuide->snap(
        p, Pk::KeyboardModifiers(static_cast<int>(modifiers)));
    if (modifiers & Pk::ControlModifier) {
        const PkRectF oldDirtyRect = d->selectedRect();
        d->selectRect.moveTopLeft(d->selectRect.topLeft() - (d->lastPos - point));
        d->lastPos = point;
        d->tool->canvas()->updateCanvas(oldDirtyRect | d->selectedRect());
        return;
    }
    d->lastPos = point;
    PkPointF old = d->selectRect.bottomRight();
    d->selectRect.setBottomRight(point);
    /*
        +---------------|--+
        |               |  |    We need to figure out rects A and B based on the two points. BUT
        |          old  | A|    we need to do that even if the points are switched places
        |             \ |  |    (i.e. the rect got smaller) and even if the rect is mirrored
        +---------------+  |    in either the horizontal or vertical axis.
        |       B          |
        +------------------+
                            `- point
    */
    PkPointF x1 = old;
    x1.setY(d->selectRect.topLeft().y());
    qreal h1 = point.y() - x1.y();
    qreal h2 = old.y() - x1.y();
    PkRectF A(x1, PkSizeF(point.x() - x1.x(), point.y() < d->selectRect.top() ? pkMin(h1, h2) : pkMax(h1, h2)));
    A = A.normalized();
    d->tool->canvas()->updateCanvas(A);

    PkPointF x2 = old;
    x2.setX(d->selectRect.topLeft().x());
    qreal w1 = point.x() - x2.x();
    qreal w2 = old.x() - x2.x();
    PkRectF B(x2, PkSizeF(point.x() < d->selectRect.left() ? pkMin(w1, w2) : pkMax(w1, w2), point.y() - x2.y()));
    B = B.normalized();
    d->tool->canvas()->updateCanvas(B);
}

KoShapeRubberSelectStrategy::SelectionMode KoShapeRubberSelectStrategy::currentMode() const
{
    Q_D(const KoShapeRubberSelectStrategy);
    return d->selectRect.left() < d->selectRect.right() ? CoveringSelection : CrossingSelection;
}

KUndo2Command *KoShapeRubberSelectStrategy::createCommand()
{
    return 0;
}

PkRectF KoShapeRubberSelectStrategy::selectedRectangle() const {
    Q_D(const KoShapeRubberSelectStrategy);
    return d->selectedRect();
}
