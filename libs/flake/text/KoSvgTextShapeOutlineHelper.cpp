/*
 *  SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KoSvgTextShapeOutlineHelper.h"

#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>

#include <KoSvgTextShape.h>
#include <KisHandlePainterHelper.h>
#include <KoShapeManager.h>
#include <KoSelection.h>
#include <QApplication>
#include <QPalette>
#include <QIcon>

const int BUTTON_ICON_SIZE = 16;
const int BUTTON_PADDING = 4;
const int BUTTON_CORNER_ROUND = 1;

struct KoSvgTextShapeOutlineHelper::Private {
    Private(KoCanvasBase *canvasBase): canvas(canvasBase) {
    }
    KoCanvasBase *canvas;
    int handleRadius = 7;
    int decorationThickness = 1;

    bool drawBoundingRect = true;
    bool drawTextWrappingArea = false;
    bool drawOutline = false;
    bool textWrappingAreasHovered = false;

    KoSvgTextShape *getTextModeShape() {
        return dynamic_cast<KoSvgTextShape*>(canvas->currentShapeManagerOwnerShape());
    }

    KoSvgTextShape *getPotentialTextShape(const PkPointF &point) {
        Q_FOREACH(KoShape*shape, canvas->shapeManager()->selection()->selectedEditableShapes()) {
            KoSvgTextShape *text = dynamic_cast<KoSvgTextShape*>(shape);
            if (drawButton(text)) {
                if (getButtonRectCorrected(text->boundingRect()).contains(point)) {
                    return text;
                }
            }
        }
        return nullptr;
    }

    KoViewConverter *converter() const {
        return canvas->viewConverter();
    }

    PkRectF getButtonRect(PkRectF base) {
        const int buttonSize = BUTTON_ICON_SIZE + (2* BUTTON_PADDING);
        return PkRectF(base.topRight(), PkSizeF(buttonSize, buttonSize));
    }
    PkRectF getButtonRectCorrected(PkRectF base) {
        return converter()->viewToDocument().mapRect(getButtonRect(converter()->documentToView().mapRect(base)));
    }

    bool drawButton(KoSvgTextShape *text) {
        return text && !text->internalShapeManager()->shapes().isEmpty();
    }
};

KoSvgTextShapeOutlineHelper::KoSvgTextShapeOutlineHelper(KoCanvasBase *canvas)
    : d(new Private(canvas))
{
}

KoSvgTextShapeOutlineHelper::~KoSvgTextShapeOutlineHelper()
{

}

PkList<PkLineF> getTextAreaOrderArrows(PkList<PkPainterPath> areas) {
    PkList<PkLineF> lines;
    if (areas.size() <= 1) return lines;
    for (int i = 1; i < areas.size(); i++) {
        const PkPainterPath previous = areas.at(i-1);
        const PkPainterPath next = areas.at(i);
        const bool overlap = previous.intersects(next);
        PkLineF arrow(previous.boundingRect().center(), next.boundingRect().center());
        if (!overlap) {

            Q_FOREACH (PkPolygonF p, previous.toSubpathPolygons()) {
                if (p.size() == 1) continue;
                for (int j = 1; j < p.size(); j++) {
                    PkLineF l2(p.at(j-1), p.at(j));
                    PkPointF intersect;
                    if (l2.intersects(arrow, &intersect) == PkLineF::BoundedIntersection) {
                        arrow.setP1(intersect);
                        break;
                    }
                }
            }
            Q_FOREACH (PkPolygonF p, next.toSubpathPolygons()) {
                if (p.size() == 1) continue;
                for (int j = 1; j < p.size(); j++) {
                    PkLineF l2(p.at(j-1), p.at(j));
                    PkPointF intersect;
                    if (l2.intersects(arrow, &intersect) == PkLineF::BoundedIntersection) {
                        arrow.setP2(intersect);
                        break;
                    }
                }
            }

        }
        lines.append(arrow);
    }
    return lines;
}

void KoSvgTextShapeOutlineHelper::paintTextShape(QPainter *painter, const KoViewConverter &converter,
                                                  const QPalette &pal, KoSvgTextShape *text,
                                                  bool contourModeActive) {
    painter->save();
    KisHandlePainterHelper helper =
            KoShape::createHandlePainterHelperView(painter, text, converter, d->handleRadius, d->decorationThickness);
    helper.setHandleStyle(KisHandleStyle::secondarySelection());
    if (contourModeActive) {
        if (d->drawOutline) {

            Q_FOREACH(KoShape *shape, text->internalShapeManager()->shapes()) {
                PkPainterPath p = shape->transformation().map(shape->outline());
                helper.drawPath(p);
            }
            PkList<PkPainterPath> areas;
            Q_FOREACH(const KoShape *shape, text->shapesInside()) {
                areas.append(shape->transformation().map(shape->outline()));
            }
            Q_FOREACH(PkLineF arrow, getTextAreaOrderArrows(areas)) {
                helper.drawGradientArrow(arrow.p1(), arrow.p2(), 1.5 * d->handleRadius);
            }
        }
        if (d->drawBoundingRect) {
            PkPainterPath rect;
            rect.addRect(text->outlineRect());
            helper.drawPath(rect);
        }
    }
    if (d->drawTextWrappingArea) {
        if (d->textWrappingAreasHovered) {
            helper.setHandleStyle(KisHandleStyle::partiallyHighlightedPrimaryHandles());
        }
        PkList<PkPainterPath> areas = text->textWrappingAreas();
        Q_FOREACH(PkLineF arrow, getTextAreaOrderArrows(areas)) {
            helper.drawGradientArrow(arrow.p1(), arrow.p2(), 1.5 * d->handleRadius);
        }
        Q_FOREACH(const PkPainterPath path, areas) {
            helper.drawPath(path);
        }
    }
    painter->restore();

    painter->save();
    QIcon icon = contourModeActive? QIcon(): QIcon();
    QPixmap pm = icon.pixmap(BUTTON_ICON_SIZE, BUTTON_ICON_SIZE);
    painter->setBrush(contourModeActive? pal.highlight(): pal.button());
    PkPen pen;
    pen.setColor(contourModeActive? pal.highlightedText().color(): pal.buttonText().color());
    pen.setCosmetic(true);
    pen.setWidthF(d->decorationThickness);
    painter->setPen(toQPen(pen));
    const PkRectF buttonRect = d->getButtonRect(converter.documentToView().mapRect(text->boundingRect()));
    painter->drawRoundedRect(buttonRect, BUTTON_CORNER_ROUND, BUTTON_CORNER_ROUND);
    painter->drawPixmap(buttonRect.topLeft()+PkPointF(BUTTON_PADDING, BUTTON_PADDING), pm);
    painter->restore();
}

void KoSvgTextShapeOutlineHelper::paint(QPainter *painter, const KoViewConverter &converter)
{
    const QPalette pal = qApp->palette();
    KoSvgTextShape *text = d->getTextModeShape();
    if (text) {
        paintTextShape(painter, converter, pal, text, true);
    } else {
        Q_FOREACH(KoShape* shape, d->canvas->shapeManager()->selection()->selectedEditableShapes()) {
            text = dynamic_cast<KoSvgTextShape*>(shape);
            if (d->drawButton(text)) {
                paintTextShape(painter, converter, pal, text, false);
            }
        }
    }
}

PkRectF KoSvgTextShapeOutlineHelper::decorationRect()
{
    PkRectF decorationRect;
    KoSvgTextShape *text = d->getTextModeShape();
    if (text) {
        PkRectF base = text->boundingRect();
        base |= d->getButtonRectCorrected(base);
        decorationRect = base;
    } else {
        Q_FOREACH(KoShape* shape, d->canvas->shapeManager()->selection()->selectedEditableShapes()) {
            text = dynamic_cast<KoSvgTextShape*>(shape);
            if (d->drawButton(text)) {
                PkRectF base = text->boundingRect();
                base |= d->getButtonRectCorrected(base);
                decorationRect |= base;
            }
        }
    }
    return decorationRect;
}

void KoSvgTextShapeOutlineHelper::setDrawBoundingRect(bool enable)
{
    d->drawBoundingRect = enable;
}

bool KoSvgTextShapeOutlineHelper::drawBoundingRect() const
{
    return d->drawBoundingRect;
}

void KoSvgTextShapeOutlineHelper::setDrawTextWrappingArea(bool enable)
{
    d->drawTextWrappingArea = enable;
}

void KoSvgTextShapeOutlineHelper::setDrawShapeOutlines(bool enable)
{
    d->drawOutline = enable;
}

bool KoSvgTextShapeOutlineHelper::drawShapeOutlines() const
{
    return d->drawOutline;
}

void KoSvgTextShapeOutlineHelper::setHandleRadius(int radius)
{
    d->handleRadius = radius;
}

void KoSvgTextShapeOutlineHelper::setDecorationThickness(int thickness)
{
    d->decorationThickness = thickness;
}

KoSvgTextShape *KoSvgTextShapeOutlineHelper::contourModeButtonHovered(const PkPointF &point)
{
    KoSvgTextShape *text = d->getTextModeShape();
    if (text) {
        if (d->getButtonRect(text->boundingRect()).contains(point)) {
            return text;
        }
    }
    return d->getPotentialTextShape(point);
}

bool KoSvgTextShapeOutlineHelper::updateTextContourMode()
{
    KoSvgTextShape *text = d->getTextModeShape();
    if (text && !d->drawButton(text)) {
        toggleTextContourMode(nullptr);
        return true;
    }
    return false;
}

void KoSvgTextShapeOutlineHelper::toggleTextContourMode(KoSvgTextShape *shape)
{
    if (d->canvas) {
        if (shape == d->canvas->currentShapeManagerOwnerShape()) {
            d->canvas->setCurrentShapeManagerOwnerShape(nullptr);
        } else {
            d->canvas->setCurrentShapeManagerOwnerShape(shape);
        }
    }
}

void KoSvgTextShapeOutlineHelper::setTextAreasHovered(bool enabled)
{
    d->textWrappingAreasHovered = enabled;
}
