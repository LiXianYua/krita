/*
 * SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "SvgTextOnPathDecorationHelper.h"

#include <KoSvgTextShape.h>
#include <KisHandlePainterHelper.h>
#include <KoViewConverter.h>
#include <KoPathShape.h>
struct SvgTextOnPathDecorationHelper::Private {
    KoSvgTextShape *shape = nullptr;
    int pos = 0;

    qreal handleRadius = 7;
    qreal decorationThickness = 1;

    bool isHovered = false;
    bool isActive = false;

    PkLineF getLineAnchorTextNode(KoSvgTextNodeIndex &index, qreal lineLength) {
        KoPathShape *s = dynamic_cast<KoPathShape*>(index.textPath());
        PkPainterPath outline = s->transformation().map(s->outline());
        PkLineF line;
        if (index.textPathInfo()->side == KoSvgText::TextPathSideRight) {
            outline = outline.toReversed();
        }
        qreal percent = index.textPathInfo()->startOffsetIsPercentage?
                    outline.percentAtLength(index.textPathInfo()->startOffset * 0.01 * outline.length()):
                    outline.percentAtLength(index.textPathInfo()->startOffset);
        if (s->isClosedSubpath(s->subpathCount()-1)) {
            percent = fmod(percent, 1.0);
        } else {
            percent = pkBound(0.0, percent, 1.0);
        }
        line.setP1(outline.pointAtPercent(percent));
        line.setAngle(outline.angleAtPercent(percent) - 90);
        line.setLength(lineLength);
        return line;
    }
};

SvgTextOnPathDecorationHelper::SvgTextOnPathDecorationHelper(): d(new Private)
{

}

SvgTextOnPathDecorationHelper::~SvgTextOnPathDecorationHelper()
{

}

void SvgTextOnPathDecorationHelper::setPos(int pos)
{
    d->pos = pos;
}

void SvgTextOnPathDecorationHelper::setShape(KoSvgTextShape *shape)
{
    d->shape = shape;
}

void SvgTextOnPathDecorationHelper::setHandleRadius(qreal radius)
{
    d->handleRadius = radius;
}

void SvgTextOnPathDecorationHelper::setDecorationThickness(qreal thickness)
{
    d->decorationThickness = thickness;
}

bool SvgTextOnPathDecorationHelper::hitTest(PkPointF mouseInPts, const PkTransform &viewToDocument)
{
    if (!d->shape) return false;
    KoSvgTextNodeIndex index = d->shape->topLevelNodeForPos(d->pos);
    if (!(index.textPath() && index.textPathInfo())) return false;
    PkPointF handleInPts = viewToDocument.map(PkPointF(d->handleRadius, d->handleRadius));

    PkLineF line = d->getLineAnchorTextNode(index, handleInPts.x()*2);
    line = d->shape->absoluteTransformation().map(line);
    bool hit = (PkLineF(line.p2(), mouseInPts).length() <= handleInPts.x()*2);
    d->isHovered = hit;
    return hit;
}

void SvgTextOnPathDecorationHelper::paint(PkPainter *p, const KoViewConverter &converter)
{
    if (!d->shape) return;
    KoSvgTextNodeIndex index = d->shape->topLevelNodeForPos(d->pos);
    if (!(index.textPath() && index.textPathInfo())) return;

    PkPointF handleInPts = converter.viewToDocument().map(PkPointF(d->handleRadius, d->handleRadius));
    PkLineF line = d->getLineAnchorTextNode(index, handleInPts.x()*3);
    p->save();
    KisHandlePainterHelper helper =
            KoShape::createHandlePainterHelperView(p, d->shape, converter, d->handleRadius, d->decorationThickness);

    if (d->isActive) {
        helper.setHandleStyle(KisHandleStyle::selectedPrimaryHandles());
    } else if (d->isHovered) {
        helper.setHandleStyle(KisHandleStyle::highlightedPrimaryHandles());
    } else {
        helper.setHandleStyle(KisHandleStyle::primarySelection());
    }

    helper.drawConnectionLine(line);
    helper.drawHandleCircle(line.p2());
    //qDebug() << line.p2();
    p->restore();
}

PkRectF SvgTextOnPathDecorationHelper::decorationRect(const PkTransform &documentToView) const
{
    PkRectF r;
    if (!d->shape) return r;
    KoSvgTextNodeIndex index = d->shape->topLevelNodeForPos(d->pos);
    if (!(index.textPath() && index.textPathInfo())) return r;
    PkPointF handleInPts = documentToView.inverted().map(PkPointF(d->handleRadius, d->handleRadius));

    PkLineF line = d->getLineAnchorTextNode(index, handleInPts.x()*3);

    line = (d->shape->absoluteTransformation()*documentToView).map(line);
    r |= PkRectF(line.p1()-PkPointF(d->decorationThickness, d->decorationThickness)
                , line.p2()+PkPointF(d->decorationThickness, d->decorationThickness));

    PkPointF handle(d->handleRadius, d->handleRadius);
    r |= PkRectF(line.p2() - handle, line.p2() + handle);

    //qDebug() << "decor rect" << r << line.p2();
    return r;
}

void SvgTextOnPathDecorationHelper::setStrategyActive(bool isActive)
{
    d->isActive = isActive;
}
