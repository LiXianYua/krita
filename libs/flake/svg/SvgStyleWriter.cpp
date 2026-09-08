/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2002 Lars Siebold <khandha5@gmx.net>
   SPDX-FileCopyrightText: 2002-2003, 2005 Rob Buis <buis@kde.org>
   SPDX-FileCopyrightText: 2002, 2005-2006 David Faure <faure@kde.org>
   SPDX-FileCopyrightText: 2002 Werner Trobin <trobin@kde.org>
   SPDX-FileCopyrightText: 2002 Lennart Kudling <kudling@kde.org>
   SPDX-FileCopyrightText: 2004 Nicolas Goutte <nicolasg@snafu.de>
   SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
   SPDX-FileCopyrightText: 2005 Raphael Langerhorst <raphael.langerhorst@kdemail.net>
   SPDX-FileCopyrightText: 2005 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2005, 2007-2008 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2006 Inge Wallin <inge@lysator.liu.se>
   SPDX-FileCopyrightText: 2006 Martin Pfeiffer <hubipete@gmx.net>
   SPDX-FileCopyrightText: 2006 Gábor Lehel <illissius@gmail.com>
   SPDX-FileCopyrightText: 2006 Laurent Montel <montel@kde.org>
   SPDX-FileCopyrightText: 2006 Christian Mueller <cmueller@gmx.de>
   SPDX-FileCopyrightText: 2006 Ariya Hidayat <ariya@kde.org>
   SPDX-FileCopyrightText: 2010 Thorsten Zachmann <zachmann@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <PkTextStream.h>
#include "SvgStyleWriter.h"
#include "SvgSavingContext.h"
#include "SvgUtil.h"
#include "shapes/ImageShapePngData.h"

#include <KoShape.h>
#include <KoPathShape.h>
#include <KoPathSegment.h>
#include <KoColorBackground.h>
#include <KoGradientBackground.h>
#include <KoMeshGradientBackground.h>
#include <KoPatternBackground.h>
#include <KoVectorPatternBackground.h>
#include <KoShapeStroke.h>
#include <KoClipPath.h>
#include <KoClipMask.h>
#include <KoMarker.h>
#include <KoXmlWriter.h>

#include <PkMemoryStream.h>
#include <PkGradient.h>
#include "kis_dom_utils.h"
#include "kis_algebra_2d.h"
#include <KisPortingUtils.h>
#include <SvgWriter.h>
#include <KoFlakeCoordinateSystem.h>


void SvgStyleWriter::saveSvgStyle(KoShape *shape, SvgSavingContext &context)
{
    saveSvgBasicStyle(shape->isVisible(false), shape->transparency(false), shape->paintOrder(), shape->inheritPaintOrder(), context);

    KoPathShape *pathShape = dynamic_cast<KoPathShape*>(shape);
    const bool fillRule = pathShape && pathShape->background() &&
        pathShape->fillRule() == Pk::OddEvenFill;
    if (!shape->inheritBackground()) {
        saveSvgFill(shape->background(), fillRule, shape->outlineRect(), shape->size(), shape->absoluteTransformation(), context);
    }
    if (!shape->inheritStroke()) {
        saveSvgStroke(shape->stroke(), context);
    }

    saveSvgClipping(shape, context);
    saveSvgMasking(shape, context);
    saveSvgMarkers(shape, context);
}

void SvgStyleWriter::saveSvgBasicStyle(const bool isVisible, const qreal transparency, const PkVector<KoShape::PaintOrder> paintOrder, bool inheritPaintorder, SvgSavingContext &context, bool textShape)
{
    if (!isVisible) {
        context.shapeWriter().addAttribute("display", "none");
    } else if (transparency > 0.0) {
        context.shapeWriter().addAttribute("opacity", 1.0 - transparency);
    }
    if (!paintOrder.isEmpty() && !inheritPaintorder) {

        const bool notDefault = paintOrder != KoShape::defaultPaintOrder();

        if ((!textShape && notDefault) || textShape) {
            PkStringList order;
            for (const KoShape::PaintOrder p : paintOrder) {
                if (p == KoShape::Fill) {
                    order.append("fill");
                } else if (p == KoShape::Stroke) {
                    order.append("stroke");
                } else if (p == KoShape::Markers) {
                    order.append("markers");
                }
            }
            context.shapeWriter().addAttribute("paint-order", order.join(" "));
        }
    }

}

void SvgStyleWriter::saveSvgFill(PkSharedPointer<KoShapeBackground> background, const bool fillRuleEvenOdd, const PkRectF outlineRect, const PkSizeF size, const PkTransform absoluteTransform, SvgSavingContext &context)
{
    if (! background) {
        context.shapeWriter().addAttribute("fill", "none");
    }

    PkSharedPointer<KoColorBackground>  cbg = pkSharedPointerDynamicCast<KoColorBackground>(background);
    if (cbg) {
        context.shapeWriter().addAttribute("fill", cbg->color().name());
        if (cbg->color().alphaF() < 1.0)
            context.shapeWriter().addAttribute("fill-opacity", cbg->color().alphaF());
    }
    PkSharedPointer<KoGradientBackground>  gbg = pkSharedPointerDynamicCast<KoGradientBackground>(background);
    if (gbg) {
        PkString gradientId = saveSvgGradient(gbg->gradient(), gbg->transform(), context);
        context.shapeWriter().addAttribute("fill", PkString("url(#" + gradientId + ")"));
    }
    PkSharedPointer<KoMeshGradientBackground> mgbg = pkSharedPointerDynamicCast<KoMeshGradientBackground>(background);
    if (mgbg) {
        PkString gradientId = saveSvgMeshGradient(mgbg->gradient(), mgbg->transform(), context);
        context.shapeWriter().addAttribute("fill", PkString("url(#" + gradientId + ")"));
    }
    PkSharedPointer<KoPatternBackground>  pbg = pkSharedPointerDynamicCast<KoPatternBackground>(background);
    if (pbg) {
        const PkString patternId = saveSvgPattern(pbg, size, absoluteTransform, context);
        context.shapeWriter().addAttribute("fill", PkString("url(#" + patternId + ")"));
    }
    PkSharedPointer<KoVectorPatternBackground>  vpbg = pkSharedPointerDynamicCast<KoVectorPatternBackground>(background);
    if (vpbg) {
        const PkString patternId = saveSvgVectorPattern(vpbg, outlineRect, context);
        context.shapeWriter().addAttribute("fill", PkString("url(#" + patternId + ")"));
    }

    // non-zero is default, so only write fillrule if evenodd is set
    if (fillRuleEvenOdd)
        context.shapeWriter().addAttribute("fill-rule", "evenodd");
}

void SvgStyleWriter::saveSvgStroke(KoShapeStrokeModelSP stroke, SvgSavingContext &context)
{
    const PkSharedPointer<KoShapeStroke> lineBorder = pkSharedPointerDynamicCast<KoShapeStroke>(stroke);

    if (! lineBorder)
        return;

    PkString strokeStr("none");
    if (lineBorder->lineBrush().gradient()) {
        PkString gradientId = saveSvgGradient(lineBorder->lineBrush().gradient(), lineBorder->lineBrush().transform(), context);
        strokeStr = "url(#" + gradientId + ")";
    } else {
        if (lineBorder->color().isValid()) {
            strokeStr = lineBorder->color().name();
        }
        if (lineBorder->color().alphaF() < 1.0) {
            context.shapeWriter().addAttribute("stroke-opacity", lineBorder->color().alphaF());
        }
    }
    if (!strokeStr.isEmpty())
        context.shapeWriter().addAttribute("stroke", strokeStr);

    context.shapeWriter().addAttribute("stroke-width", SvgUtil::toUserSpace(lineBorder->lineWidth()));

    if (lineBorder->capStyle() == Pk::FlatCap)
        context.shapeWriter().addAttribute("stroke-linecap", "butt");
    else if (lineBorder->capStyle() == Pk::RoundCap)
        context.shapeWriter().addAttribute("stroke-linecap", "round");
    else if (lineBorder->capStyle() == Pk::SquareCap)
        context.shapeWriter().addAttribute("stroke-linecap", "square");

    if (lineBorder->joinStyle() == Pk::MiterJoin) {
        context.shapeWriter().addAttribute("stroke-linejoin", "miter");
        context.shapeWriter().addAttribute("stroke-miterlimit", lineBorder->miterLimit());
    } else if (lineBorder->joinStyle() == Pk::RoundJoin)
        context.shapeWriter().addAttribute("stroke-linejoin", "round");
    else if (lineBorder->joinStyle() == Pk::BevelJoin)
        context.shapeWriter().addAttribute("stroke-linejoin", "bevel");

    // dash
    if (lineBorder->lineStyle() > Pk::SolidLine) {
        qreal dashFactor = lineBorder->lineWidth();

        if (lineBorder->dashOffset() != 0)
            context.shapeWriter().addAttribute("stroke-dashoffset", dashFactor * lineBorder->dashOffset());

        PkString dashStr;
        const PkVector<qreal> dashes = lineBorder->lineDashes();
        int dashCount = dashes.size();
        for (int i = 0; i < dashCount; ++i) {
            if (i > 0)
                dashStr += ",";
            dashStr += PkString("%1").arg(KisDomUtils::toString(dashes[i] * dashFactor));
        }
        context.shapeWriter().addAttribute("stroke-dasharray", dashStr);
    }
}

void embedShapes(const PkList<KoShape*> &shapes, KoXmlWriter &outWriter)
{
    PkMemoryStream buffer;
    buffer.open(PkStream::WriteOnly);
    {
        SvgWriter shapesWriter(shapes);
        shapesWriter.saveDetached(buffer);
    }
    buffer.close();
    buffer.open(PkStream::ReadOnly);
    outWriter.addCompleteElement(&buffer);
}

PkString SvgStyleWriter::embedShape(const KoShape *shape, SvgSavingContext &context)
{
    PkList<KoShape *> shapes;
    KoShape* clonedShape = shape->cloneShape();
    if (!clonedShape) {
        return PkString();
    }
    const PkString uid = context.createUID("path");
    clonedShape->setName(uid);
    shapes.append(clonedShape);
    embedShapes(shapes, context.styleWriter());
    return uid;
}

void SvgStyleWriter::saveMetadata(const KoShape *shape, SvgSavingContext &context)
{
    const PkString title = shape->additionalAttribute("title");
    if (!title.trimmed().isEmpty()) {
        context.shapeWriter().startElement("title");
        context.shapeWriter().addTextNode(title);
        context.shapeWriter().endElement();
    }
    const PkString desc = shape->additionalAttribute("desc");
    if (!desc.trimmed().isEmpty()) {
        context.shapeWriter().startElement("desc");
        context.shapeWriter().addTextNode(desc);
        context.shapeWriter().endElement();
    }
}

void SvgStyleWriter::saveSvgClipping(KoShape *shape, SvgSavingContext &context)
{
    KoClipPath *clipPath = shape->clipPath();
    if (!clipPath)
        return;

    const PkString uid = context.createUID("clippath");

    context.styleWriter().startElement("clipPath");
    context.styleWriter().addAttribute("id", uid);
    context.styleWriter().addAttribute("clipPathUnits", KoFlake::coordinateToString(clipPath->coordinates()));

    embedShapes(clipPath->clipShapes(), context.styleWriter());

    context.styleWriter().endElement(); // clipPath

    context.shapeWriter().addAttribute("clip-path", PkString("url(#" + uid + ")"));
    if (clipPath->clipRule() != Pk::WindingFill)
        context.shapeWriter().addAttribute("clip-rule", "evenodd");
}

void SvgStyleWriter::saveSvgMasking(KoShape *shape, SvgSavingContext &context)
{
    KoClipMask*clipMask = shape->clipMask();
    if (!clipMask)
        return;

    const PkString uid = context.createUID("clipmask");

    context.styleWriter().startElement("mask");
    context.styleWriter().addAttribute("id", uid);
    context.styleWriter().addAttribute("maskUnits", KoFlake::coordinateToString(clipMask->coordinates()));
    context.styleWriter().addAttribute("maskContentUnits", KoFlake::coordinateToString(clipMask->contentCoordinates()));

    const PkRectF rect = clipMask->maskRect();

    context.styleWriter().addAttribute("x", rect.x());
    context.styleWriter().addAttribute("y", rect.y());
    context.styleWriter().addAttribute("width", rect.width());
    context.styleWriter().addAttribute("height", rect.height());

    embedShapes(clipMask->shapes(), context.styleWriter());

    context.styleWriter().endElement(); // clipMask

    context.shapeWriter().addAttribute("mask", PkString("url(#" + uid + ")"));
}

namespace {
void writeMarkerStyle(KoXmlWriter &styleWriter, const KoMarker *marker, const PkString &assignedId) {

    styleWriter.startElement("marker");
    styleWriter.addAttribute("id", assignedId);
    styleWriter.addAttribute("markerUnits", KoMarker::coordinateSystemToString(marker->coordinateSystem()));

    const PkPointF refPoint = marker->referencePoint();
    styleWriter.addAttribute("refX", refPoint.x());
    styleWriter.addAttribute("refY", refPoint.y());

    const PkSizeF refSize = marker->referenceSize();
    styleWriter.addAttribute("markerWidth", refSize.width());
    styleWriter.addAttribute("markerHeight", refSize.height());


    if (marker->hasAutoOrientation()) {
        styleWriter.addAttribute("orient", "auto");
    } else {
        // no suffix means 'degrees'
        styleWriter.addAttribute("orient", kisRadiansToDegrees(marker->explicitOrientation()));
    }

    embedShapes(marker->shapes(), styleWriter);

    styleWriter.endElement(); // marker
}

void tryEmbedMarker(const KoPathShape *pathShape,
                    const PkString &markerTag,
                    KoFlake::MarkerPosition markerPosition,
                    SvgSavingContext &context)
{
    KoMarker *marker = pathShape->marker(markerPosition);

    if (marker) {
        const PkString uid = context.createUID("lineMarker");
        writeMarkerStyle(context.styleWriter(), marker, uid);
        context.shapeWriter().addAttribute(markerTag.toLatin1().data(), PkString("url(#" + uid + ")"));
    }
}

}

void SvgStyleWriter::saveSvgMarkers(KoShape *shape, SvgSavingContext &context)
{
    KoPathShape *pathShape = dynamic_cast<KoPathShape*>(shape);
    if (!pathShape || !pathShape->hasMarkers()) return;


    tryEmbedMarker(pathShape, "marker-start", KoFlake::StartMarker, context);
    tryEmbedMarker(pathShape, "marker-mid", KoFlake::MidMarker, context);
    tryEmbedMarker(pathShape, "marker-end", KoFlake::EndMarker, context);

    if (pathShape->autoFillMarkers()) {
        context.shapeWriter().addAttribute("krita:marker-fill-method", "auto");
    }
}

void SvgStyleWriter::saveSvgColorStops(const PkGradientStops &colorStops, SvgSavingContext &context)
{
    for (const PkGradientStop &stop : colorStops) {
        context.styleWriter().startElement("stop");
        context.styleWriter().addAttribute("stop-color", stop.color.name());
        context.styleWriter().addAttribute("offset", stop.offset);
        context.styleWriter().addAttribute("stop-opacity", stop.color.alphaF());
        context.styleWriter().endElement();
    }
}

inline PkString convertGradientMode(PkGradientEnums::CoordinateMode mode) {
    // StretchToDeviceMode 未进 PkGradientEnums（S 线裁剪，S-09-g）

    return
        mode == PkGradientEnums::ObjectBoundingMode ?
        "objectBoundingBox" :
        "userSpaceOnUse";

}

PkString SvgStyleWriter::saveSvgGradient(const PkGradient *gradient, const PkTransform &gradientTransform, SvgSavingContext &context)
{
    if (! gradient)
        return PkString();

    const PkString spreadMethod[3] = {
        PkString("pad"),
        PkString("reflect"),
        PkString("repeat")
    };

    const PkString uid = context.createUID("gradient");

    if (gradient->type() == PkGradient::LinearGradient) {
        const PkGradient * g = gradient;
        context.styleWriter().startElement("linearGradient");
        context.styleWriter().addAttribute("id", uid);
        SvgUtil::writeTransformAttributeLazy("gradientTransform", gradientTransform, context.styleWriter());
        context.styleWriter().addAttribute("gradientUnits", convertGradientMode(g->coordinateMode()));
        context.styleWriter().addAttribute("x1", g->start().x());
        context.styleWriter().addAttribute("y1", g->start().y());
        context.styleWriter().addAttribute("x2", g->finalStop().x());
        context.styleWriter().addAttribute("y2", g->finalStop().y());
        context.styleWriter().addAttribute("spreadMethod", spreadMethod[g->spread()]);
        // color stops
        saveSvgColorStops(gradient->stops(), context);
        context.styleWriter().endElement();
    } else if (gradient->type() == PkGradient::RadialGradient) {
        const PkGradient * g = gradient;
        context.styleWriter().startElement("radialGradient");
        context.styleWriter().addAttribute("id", uid);
        SvgUtil::writeTransformAttributeLazy("gradientTransform", gradientTransform, context.styleWriter());
        context.styleWriter().addAttribute("gradientUnits", convertGradientMode(g->coordinateMode()));
        context.styleWriter().addAttribute("cx", g->center().x());
        context.styleWriter().addAttribute("cy", g->center().y());
        context.styleWriter().addAttribute("fx", g->focalPoint().x());
        context.styleWriter().addAttribute("fy", g->focalPoint().y());
        context.styleWriter().addAttribute("r", g->radius());
        context.styleWriter().addAttribute("spreadMethod", spreadMethod[g->spread()]);
        // color stops
        saveSvgColorStops(gradient->stops(), context);
        context.styleWriter().endElement();
    } else if (gradient->type() == PkGradient::ConicalGradient) {
        // A conical gradient has no SVG 1.1 equivalent; approximate as radial.
        // fake conical grad as radial.
        // fugly but better than data loss.
        /*
        printIndentation( m_defs, m_indent2 );
        *m_defs << "<radialGradient id=\"" << uid << "\" ";
        *m_defs << "gradientUnits=\"userSpaceOnUse\" ";
        *m_defs << "cx=\"" << g->center().x() << "\" ";
        *m_defs << "cy=\"" << g->center().y() << "\" ";
        *m_defs << "fx=\"" << grad.focalPoint().x() << "\" ";
        *m_defs << "fy=\"" << grad.focalPoint().y() << "\" ";
        double r = sqrt( pow( grad.vector().x() - grad.origin().x(), 2 ) + pow( grad.vector().y() - grad.origin().y(), 2 ) );
        *m_defs << "r=\"" << PkString().setNum( r ) << "\" ";
        *m_defs << spreadMethod[g->spread()];
        *m_defs << ">\n";

        // color stops
        getColorStops( gradient->stops() );

        printIndentation( m_defs, m_indent2 );
        *m_defs << "</radialGradient>\n";
        *m_body << "url(#" << uid << ")";
        */
    }

    return uid;
}

PkString SvgStyleWriter::saveSvgMeshGradient(SvgMeshGradient *gradient,
                                            const PkTransform& transform,
                                            SvgSavingContext &context)
{
    if (!gradient || !gradient->isValid())
        return PkString();

    const PkString uid = context.createUID("meshgradient");
    context.styleWriter().startElement("meshgradient");
    context.styleWriter().addAttribute("id", uid);

    if (gradient->gradientUnits() == KoFlake::ObjectBoundingBox) {
        context.styleWriter().addAttribute("gradientUnits", "objectBoundingBox");
    } else {
        context.styleWriter().addAttribute("gradientUnits", "userSpaceOnUse");
    }

    SvgUtil::writeTransformAttributeLazy("transform", transform, context.styleWriter());

    SvgMeshArray *mesharray = gradient->getMeshArray().data();
    PkPointF start = mesharray->getPatch(0, 0)->getStop(SvgMeshPatch::Top).point;

    context.styleWriter().addAttribute("x", start.x());
    context.styleWriter().addAttribute("y", start.y());

    if (gradient->type() == SvgMeshGradient::BILINEAR) {
        context.styleWriter().addAttribute("type", "bilinear");
    } else {
        context.styleWriter().addAttribute("type", "bicubic");
    }

    for (int row = 0; row < mesharray->numRows(); ++row) {

        const PkString uid = context.createUID("meshrow");
        context.styleWriter().startElement("meshrow");
        context.styleWriter().addAttribute("id", uid);

        for (int col = 0; col < mesharray->numColumns(); ++col) {

            const PkString uid = context.createUID("meshpatch");
            context.styleWriter().startElement("meshpatch");
            context.styleWriter().addAttribute("id", uid);

            SvgMeshPatch *patch = mesharray->getPatch(row, col);

            for (int s = 0; s < 4; ++s) {
                SvgMeshPatch::Type type = static_cast<SvgMeshPatch::Type> (s);

                // only first row and first col have Top and Left stop, respectively
                if ((row != 0 && s == SvgMeshPatch::Top) ||
                    (col != 0 && s == SvgMeshPatch::Left)) {
                    continue;
                }

                context.styleWriter().startElement("stop");

                const std::array<PkPointF, 4> pkSegment = patch->getSegment(type);
                std::array<PkPointF, 4> segment;
                for (int i = 0; i < 4; ++i) {
                    segment[i] = pkSegment[i];
                }

                PkString pathstr;
                PkTextStream stream(&pathstr);

                stream.setRealNumberPrecision(10);
                // TODO: other path type?
                stream << "C "
                       << segment[1].x() << "," << segment[1].y() << " "
                       << segment[2].x() << "," << segment[2].y() << " "
                       << segment[3].x() << "," << segment[3].y(); // I don't see any harm, inkscape does this too

                context.styleWriter().addAttribute("path", pathstr);

                // don't add color/opacity if stop is in first row and stop == Top (or)
                // don't add color/opacity if stop is not in first row and stop == Right
                if ((row != 0 || col == 0 || s != SvgMeshPatch::Top) &&
                    (row == 0 || s != SvgMeshPatch::Right)) {

                    SvgMeshStop stop = patch->getStop(type);
                    context.styleWriter().addAttribute("stop-color", stop.color.name());
                    context.styleWriter().addAttribute("stop-opacity", stop.color.alphaF());
                }

                context.styleWriter().endElement(); // stop
            }

            context.styleWriter().endElement();  // meshpatch
        }
        context.styleWriter().endElement(); // meshrow
    }
    context.styleWriter().endElement(); // meshgradient

    return uid;
}

PkString SvgStyleWriter::saveSvgPattern(PkSharedPointer<KoPatternBackground> pattern, const PkSizeF shapeSize, const PkTransform absoluteTransform, SvgSavingContext &context)
{
    const PkString uid = context.createUID("pattern");

    const PkSizeF patternSize = pattern->patternDisplaySize();
    const PkSize imageSize = pattern->pattern().size();

    // calculate offset in point
    PkPointF offset = pattern->referencePointOffset();
    offset.rx() = 0.01 * offset.x() * patternSize.width();
    offset.ry() = 0.01 * offset.y() * patternSize.height();

    // now take the reference point into account
    switch (pattern->referencePoint()) {
    case KoPatternBackground::TopLeft:
        break;
    case KoPatternBackground::Top:
        offset += PkPointF(0.5 * shapeSize.width(), 0.0);
        break;
    case KoPatternBackground::TopRight:
        offset += PkPointF(shapeSize.width(), 0.0);
        break;
    case KoPatternBackground::Left:
        offset += PkPointF(0.0, 0.5 * shapeSize.height());
        break;
    case KoPatternBackground::Center:
        offset += PkPointF(0.5 * shapeSize.width(), 0.5 * shapeSize.height());
        break;
    case KoPatternBackground::Right:
        offset += PkPointF(shapeSize.width(), 0.5 * shapeSize.height());
        break;
    case KoPatternBackground::BottomLeft:
        offset += PkPointF(0.0, shapeSize.height());
        break;
    case KoPatternBackground::Bottom:
        offset += PkPointF(0.5 * shapeSize.width(), shapeSize.height());
        break;
    case KoPatternBackground::BottomRight:
        offset += PkPointF(shapeSize.width(), shapeSize.height());
        break;
    }

    offset = absoluteTransform.map(offset);

    context.styleWriter().startElement("pattern");
    context.styleWriter().addAttribute("id", uid);
    context.styleWriter().addAttribute("x", SvgUtil::toUserSpace(offset.x()));
    context.styleWriter().addAttribute("y", SvgUtil::toUserSpace(offset.y()));

    if (pattern->repeat() == KoPatternBackground::Stretched) {
        context.styleWriter().addAttribute("width", "100%");
        context.styleWriter().addAttribute("height", "100%");
        context.styleWriter().addAttribute("patternUnits", "objectBoundingBox");
    } else {
        context.styleWriter().addAttribute("width", SvgUtil::toUserSpace(patternSize.width()));
        context.styleWriter().addAttribute("height", SvgUtil::toUserSpace(patternSize.height()));
        context.styleWriter().addAttribute("patternUnits", "userSpaceOnUse");
    }

    context.styleWriter().addAttribute("viewBox", PkString("0 0 %1 %2").arg(KisDomUtils::toString(imageSize.width())).arg(KisDomUtils::toString(imageSize.height())));
    //*m_defs << " patternContentUnits=\"userSpaceOnUse\"";

    context.styleWriter().startElement("image");
    context.styleWriter().addAttribute("x", "0");
    context.styleWriter().addAttribute("y", "0");
    context.styleWriter().addAttribute("width", PkString("%1px").arg(KisDomUtils::toString(imageSize.width())));
    context.styleWriter().addAttribute("height", PkString("%1px").arg(KisDomUtils::toString(imageSize.height())));

    const PkString dataUri = ImageShapePngData::encodeDataUri(pattern->pattern());
    if (!dataUri.isEmpty()) {
        context.styleWriter().addAttribute("xlink:href", dataUri);
    }

    context.styleWriter().endElement(); // image
    context.styleWriter().endElement(); // pattern

    return uid;
}

PkString SvgStyleWriter::saveSvgVectorPattern(PkSharedPointer<KoVectorPatternBackground> pattern, const PkRectF outlineRect, SvgSavingContext &context)
{
    const PkString uid = context.createUID("pattern");

    context.styleWriter().startElement("pattern");
    context.styleWriter().addAttribute("id", uid);

    context.styleWriter().addAttribute("patternUnits", KoFlake::coordinateToString(pattern->referenceCoordinates()));
    context.styleWriter().addAttribute("patternContentUnits", KoFlake::coordinateToString(pattern->contentCoordinates()));

    const PkRectF rect = pattern->referenceRect();

    context.styleWriter().addAttribute("x", rect.x());
    context.styleWriter().addAttribute("y", rect.y());
    context.styleWriter().addAttribute("width", rect.width());
    context.styleWriter().addAttribute("height", rect.height());

    SvgUtil::writeTransformAttributeLazy("patternTransform", pattern->patternTransform(), context.styleWriter());

    if (pattern->contentCoordinates() == KoFlake::ObjectBoundingBox) {
        // TODO: move this normalization into the KoVectorPatternBackground itself

        PkList<KoShape*> shapes = pattern->shapes();
        PkList<KoShape*> clonedShapes;

        const PkTransform relativeToShape = KisAlgebra2D::mapToRect(outlineRect);
        const PkTransform shapeToRelative = relativeToShape.inverted();

        for (KoShape *shape : shapes) {
            KoShape *clone = shape->cloneShape();
            clone->applyAbsoluteTransformation(shapeToRelative);
            clonedShapes.append(clone);
        }

        embedShapes(clonedShapes, context.styleWriter());
        for (KoShape *shape : clonedShapes) {
            delete shape;
        }

    } else {
        PkList<KoShape*> shapes = pattern->shapes();
        embedShapes(shapes, context.styleWriter());
    }

    context.styleWriter().endElement(); // pattern

    return uid;
}
