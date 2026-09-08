/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2022 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include <PkGradient.h>
#include "KoSvgTextShape.h"
#include "KoSvgTextShape_p.h"

#include "KoSvgTextProperties.h"

#include <KoClipMaskPainter.h>
#include <KoColorBackground.h>
#include <KoGradientBackground.h>
#include <KoPathShape.h>
#include <KoShapeStroke.h>
#include <KoShapeRegistry.h>
#include <KoShapeFactoryBase.h>
#include <KoProperties.h>
#include <KoClipMask.h>
#include <KoInsets.h>
#include <KoShapeGroup.h>
#include <KoShapeGroupCommand.h>

#include <kis_algebra_2d.h>

#include <PkPainter.h>
#include <QtMath>

#include <variant>

static void inheritPaintProperties(const KisForest<KoSvgTextContentElement>::composition_iterator it,
                       KoShapeStrokeModelSP &stroke,
                       PkSharedPointer<KoShapeBackground> &background,
                       PkVector<KoShape::PaintOrder> &paintOrder) {
    for (auto parentIt = KisForestDetail::hierarchyBegin(siblingCurrent(it)); parentIt != KisForestDetail::hierarchyEnd(siblingCurrent(it)); parentIt++) {
        if (parentIt->properties.hasProperty(KoSvgTextProperties::StrokeId)) {
            stroke = parentIt->properties.stroke();
            break;
        }
    }
    for (auto parentIt = KisForestDetail::hierarchyBegin(siblingCurrent(it)); parentIt != KisForestDetail::hierarchyEnd(siblingCurrent(it)); parentIt++) {
        if (parentIt->properties.hasProperty(KoSvgTextProperties::FillId)) {
            background = parentIt->properties.background();
            break;
        }
    }
    for (auto parentIt = KisForestDetail::hierarchyBegin(siblingCurrent(it)); parentIt != KisForestDetail::hierarchyEnd(siblingCurrent(it)); parentIt++) {
        if (parentIt->properties.hasProperty(KoSvgTextProperties::PaintOrder)) {
            paintOrder = parentIt->properties.propertyOrDefault(KoSvgTextProperties::PaintOrder).value<PkVector<KoShape::PaintOrder>>();
            break;
        }
    }
}

void setRenderHints(PkPainter &painter, const KoSvgText::TextRendering textRendering, const bool testAntialiasing) {
    if (textRendering != KoSvgText::RenderingOptimizeSpeed && testAntialiasing) {
        // also apply antialiasing only if antialiasing is active on provided target PkPainter
        painter.setRenderHint(PkPainter::Antialiasing, true);
        painter.setRenderHint(PkPainter::SmoothPixmapTransform, true);
    } else {
        painter.setRenderHint(PkPainter::Antialiasing, false);
        painter.setRenderHint(PkPainter::SmoothPixmapTransform, false);
    }
}

void KoSvgTextShape::Private::paintTextDecoration(PkPainter &painter,
                                                  const PkPainterPath &rootOutline,
                                                  const KoShape *rootShape,
                                                  const KoSvgText::TextDecoration type,
                                                  const KoSvgText::TextRendering rendering)
{
    KoShapeStrokeModelSP stroke = rootShape->stroke();
    PkSharedPointer<KoShapeBackground> background = rootShape->background();
    PkVector<KoShape::PaintOrder> paintOrder = rootShape->paintOrder();

    for (auto it = compositionBegin(textData); it != compositionEnd(textData); it++) {
        if (it.state() == KisForestDetail::Leave) continue;

        inheritPaintProperties(it, stroke, background, paintOrder);


        KoInsets insets;
        if (stroke) {
            stroke->strokeInsets(rootShape, insets);
        }
        PkMap<KoSvgText::TextDecoration, PkPainterPath> textDecorations = it->textDecorations;

        if (textDecorations.isEmpty() || !textDecorations.contains(type)) continue;

        const PkPainterPath decorPath = textDecorations.value(type);
        const PkRect shapeGlobalClipRect = painter.transform().mapRect(decorPath.boundingRect().adjusted(-insets.left, -insets.top, insets.right, insets.bottom)).toAlignedRect();

        if (!shapeGlobalClipRect.isValid()) continue;

        const PkRectF clipRect = painter.clipBoundingRect();
        if (!clipRect.contains(decorPath.boundingRect()) &&
                 !clipRect.intersects(decorPath.boundingRect())) continue;
        const PkColor textDecorationColor = it->properties.propertyOrDefault(KoSvgTextProperties::TextDecorationColorId).value<PkColor>();
        const bool colorValid = textDecorationColor.isValid() && it->properties.hasProperty(KoSvgTextProperties::TextDecorationColorId) && textDecorationColor != PkColor(Pk::transparent);

        Q_FOREACH(const KoShape::PaintOrder p, paintOrder) {
            if (p == KoShape::Fill) {

                if (background && !colorValid) {
                    KoClipMaskPainter fillPainter(&painter, PkRectF(shapeGlobalClipRect));
                    setRenderHints(*fillPainter.maskPainter(), rendering, painter.testRenderHint(PkPainter::Antialiasing));
                    background->paint(*fillPainter.shapePainter(), rootOutline);
                    fillPainter.maskPainter()->fillPath(rootOutline, PkBrush(PkColor(Pk::black)));
                    fillPainter.maskPainter()->fillPath(decorPath, Pk::white);
                    fillPainter.renderOnGlobalPainter();
                } else if (colorValid) {
                    painter.fillPath(decorPath, textDecorationColor);
                }
            } else if (p == KoShape::Stroke) {
                if (stroke) {
                    KoShapeStrokeSP strokeSP = pkSharedPointerDynamicCast<KoShapeStroke>(stroke);

                    if (strokeSP) {
                        if (strokeSP->lineBrush().gradient()) {
                            KoClipMaskPainter strokePainter(&painter, PkRectF(shapeGlobalClipRect));
                            PkPainterPath strokeOutline;
                            strokeOutline.addRect(rootOutline.boundingRect().adjusted(-insets.left, -insets.top, insets.right, insets.bottom));
                            strokePainter.shapePainter()->fillRect(strokeOutline.boundingRect(), strokeSP->lineBrush());
                            strokePainter.maskPainter()->fillRect(strokeOutline.boundingRect(), PkBrush(PkColor(Pk::black)));

                            KoShapeStrokeSP maskStroke = KoShapeStrokeSP(new KoShapeStroke(*strokeSP.data()));
                            maskStroke->setColor(PkColor(Pk::white));
                            maskStroke->setLineBrush(PkBrush(PkColor(Pk::white)));


                            setRenderHints(*strokePainter.maskPainter(), rendering, painter.testRenderHint(PkPainter::Antialiasing));
                            {
                                PkScopedPointer<KoShape> shape(KoPathShape::createShapeFromPainterPath(decorPath));
                                maskStroke->paint(shape.data(), *strokePainter.maskPainter());
                            }
                            strokePainter.renderOnGlobalPainter();
                        } else {
                            {
                                PkScopedPointer<KoShape> shape(KoPathShape::createShapeFromPainterPath(decorPath));
                                stroke->paint(shape.data(), painter);
                            }
                        }
                    }
                }
            }
        }

    }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void KoSvgTextShape::Private::paintPaths(PkPainter &painter,
                                         const PkPainterPath &rootOutline,
                                         const KoShape *rootShape,
                                         const PkVector<CharacterResult> &result, const KoSvgText::TextRendering rendering,
                                         PkPainterPath &chunk,
                                         int &currentIndex)
{

    KoShapeStrokeModelSP stroke = rootShape->stroke();
    PkSharedPointer<KoShapeBackground> background = rootShape->background();
    PkVector<KoShape::PaintOrder> paintOrder = rootShape->paintOrder();

    for (auto it = compositionBegin(textData); it != compositionEnd(textData); it++) {
        inheritPaintProperties(it, stroke, background, paintOrder);

        KoInsets insets;
        if (stroke) {
            stroke->strokeInsets(rootShape, insets);
        }

        if (it.state() == KisForestDetail::Enter) {

            if (childCount(siblingCurrent(it)) == 0) {
                if (it->finalResultIndex < 0) continue;
                const int j = it->finalResultIndex;//currentIndex + it->numChars(true);

                const PkRect shapeGlobalClipRect = painter.transform().mapRect(it->associatedOutline.boundingRect().adjusted(-insets.left, -insets.top, insets.right, insets.bottom)).toAlignedRect();

                if (shapeGlobalClipRect.isValid()) {
                    KoClipMaskPainter fillPainter(&painter, PkRectF(shapeGlobalClipRect));
                    if (background) {
                        background->paint(*fillPainter.shapePainter(), rootOutline);
                        fillPainter.maskPainter()->fillPath(rootOutline, PkBrush(PkColor(Pk::black)));
                        setRenderHints(*fillPainter.maskPainter(), rendering, painter.testRenderHint(PkPainter::Antialiasing));
                    }
                    PkPainterPath textDecorationsRest;
                    textDecorationsRest.setFillRule(Pk::WindingFill);

                    for (int i = currentIndex; i < j; i++) {
                        if (result.at(i).addressable && !result.at(i).hidden) {
                            const PkTransform tf = result.at(i).finalTransform();

                            /**
                     * Make sure the character touches the painter's clip rect,
                     * otherwise we can just skip it. Adding insets to ensure the outline will not be skipped.
                     */
                            const PkRectF boundingRect = tf.mapRect(result.at(i).inkBoundingBox).adjusted(-insets.left, -insets.top, insets.right, insets.bottom);
                            const PkRectF clipRect = painter.clipBoundingRect();
                            if (boundingRect.isEmpty() ||
                                    (!clipRect.contains(boundingRect) &&
                                     !clipRect.intersects(boundingRect))) continue;

                            /**
                     * There's an annoying problem here that officially speaking
                     * the chunks need to be unified into one single path before
                     * drawing, so there's no weirdness with the stroke, but
                     * PkPainterPath's union function will frequently lead to
                     * reduced quality of the paths because of 'numerical
                     * instability'.
                     */

                            if (const auto *colorGlyph = std::get_if<Glyph::ColorLayers>(&result.at(i).glyph)) {
                                for (int c = 0; c < colorGlyph->paths.size(); c++) {
                                    PkBrush color = colorGlyph->colors.at(c);
                                    bool replace = colorGlyph->replaceWithForeGroundColor.at(c);
                                    // In theory we can use the pattern or gradient as well
                                    // for ColorV0 fonts, but ColorV1 fonts can have
                                    // gradients, so I am hesitant.
                                    KoColorBackground *b = dynamic_cast<KoColorBackground *>(background.data());
                                    if (b && replace) {
                                        color = b->brush();
                                    }
                                    painter.fillPath(tf.map(colorGlyph->paths.at(c)), color);
                                }
                            } else if (const auto *outlineGlyph = std::get_if<Glyph::Outline>(&result.at(i).glyph)) {
                                chunk.addPath(tf.map(outlineGlyph->path));
                            } else if (const auto *bitmapGlyph = std::get_if<Glyph::Bitmap>(&result.at(i).glyph)) {
                                for (int b = 0; b < bitmapGlyph->images.size(); b++) {
                                    PkImage img = bitmapGlyph->images.at(b);
                                    PkRectF rect = bitmapGlyph->drawRects.value(b, PkRectF(0, 0, img.width(), img.height()));
                                    if (img.format() == PkImage::Format_Grayscale8 || img.format() == PkImage::Format_Mono) {
                                        fillPainter.maskPainter()->save();
                                        fillPainter.maskPainter()->translate(result.at(i).finalPosition.x(), result.at(i).finalPosition.y());
                                        fillPainter.maskPainter()->rotate(qRadiansToDegrees(result.at(i).rotate));
                                        fillPainter.maskPainter()->setCompositionMode(Pk::CompositionMode_Plus);
                                        fillPainter.maskPainter()->drawImage(rect, img);
                                        fillPainter.maskPainter()->restore();
                                    } else {
                                        painter.save();
                                        painter.translate(result.at(i).finalPosition.x(), result.at(i).finalPosition.y());
                                        painter.rotate(qRadiansToDegrees(result.at(i).rotate));
                                        painter.setRenderHint(PkPainter::SmoothPixmapTransform, true);
                                        painter.drawImage(rect, img);
                                        painter.restore();
                                    }
                                }
                            }
                        }
                    }
                    Q_FOREACH(const KoShape::PaintOrder p, paintOrder) {
                        if (p == KoShape::Fill) {
                            if (background) {
                                chunk.setFillRule(Pk::WindingFill);
                                fillPainter.maskPainter()->fillPath(chunk, Pk::white);
                            }
                            if (!textDecorationsRest.isEmpty()) {
                                fillPainter.maskPainter()->fillPath(textDecorationsRest.simplified(), PkBrush(PkColor(Pk::white)));
                            }
                            fillPainter.renderOnGlobalPainter();
                        } else if (p == KoShape::Stroke) {
                            KoShapeStrokeSP maskStroke;
                            if (stroke) {
                                KoShapeStrokeSP strokeSP = pkSharedPointerDynamicCast<KoShapeStroke>(stroke);

                                if (strokeSP) {
                                    if (strokeSP->lineBrush().gradient()) {
                                        KoClipMaskPainter strokePainter(&painter, PkRectF(shapeGlobalClipRect));
                                        PkPainterPath strokeOutline;
                                        strokeOutline.addRect(rootOutline.boundingRect().adjusted(-insets.left, -insets.top, insets.right, insets.bottom));
                                        strokePainter.shapePainter()->fillRect(strokeOutline.boundingRect(), strokeSP->lineBrush());
                                        strokePainter.maskPainter()->fillRect(strokeOutline.boundingRect(), PkBrush(PkColor(Pk::black)));
                                        maskStroke = KoShapeStrokeSP(new KoShapeStroke(*strokeSP.data()));
                                        maskStroke->setColor(PkColor(Pk::white));
                                        maskStroke->setLineBrush(PkBrush(PkColor(Pk::white)));
                                        setRenderHints(*strokePainter.maskPainter(), rendering, painter.testRenderHint(PkPainter::Antialiasing));
                                        {
                                            PkScopedPointer<KoShape> shape(KoPathShape::createShapeFromPainterPath(chunk));
                                            maskStroke->paint(shape.data(), *strokePainter.maskPainter());
                                        }
                                        if (!textDecorationsRest.isEmpty()) {
                                            PkScopedPointer<KoShape> shape(KoPathShape::createShapeFromPainterPath(textDecorationsRest));
                                            maskStroke->paint(shape.data(), *strokePainter.maskPainter());
                                        }
                                        strokePainter.renderOnGlobalPainter();
                                    } else {
                                        {
                                            PkScopedPointer<KoShape> shape(KoPathShape::createShapeFromPainterPath(chunk));
                                            stroke->paint(shape.data(), painter);
                                        }
                                        if (!textDecorationsRest.isEmpty()) {
                                            PkScopedPointer<KoShape> shape(KoPathShape::createShapeFromPainterPath(textDecorationsRest));
                                            stroke->paint(shape.data(), painter);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                chunk = PkPainterPath();
                currentIndex = j;
            }
        }
    }
}

PkGradient *cloneAndTransformGradient(const PkGradient *grad, const PkTransform &tf) {
    PkGradient *newGrad = KoFlake::cloneGradient(grad);

    if (newGrad->type() == PkGradient::LinearGradient) {
        PkGradient *lgradient = newGrad;
        lgradient->setStart(tf.map(lgradient->start()));
        lgradient->setFinalStop(tf.map(lgradient->finalStop()));
    } else if (newGrad->type() == PkGradient::RadialGradient) {
        PkGradient *rgradient = newGrad;
        rgradient->setFocalPoint(tf.map(rgradient->focalPoint()));
        rgradient->setCenter(tf.map(rgradient->center()));
    }
    return newGrad;
}

PkSharedPointer<KoShapeBackground> transformBackgroundToBounds(PkSharedPointer<KoShapeBackground> bg, const PkRectF &oldBounds, const PkRectF &newBounds) {
    KoGradientBackground *g = dynamic_cast<KoGradientBackground *>(bg.data());

    if (g) {
        PkRectF relative = KisAlgebra2D::absoluteToRelative(oldBounds, newBounds);
        PkTransform newTf = PkTransform::fromTranslate(relative.x(), relative.y());
        newTf.scale(relative.width(), relative.height());

        return PkSharedPointer<KoGradientBackground>(new KoGradientBackground(cloneAndTransformGradient(g->gradient(), newTf), g->transform()));
    }

    // assume bg is KoColorBackground.
    return bg;
}

KoShapeStrokeModelSP transformStrokeBgToNewBounds(KoShapeStrokeModelSP stroke, const PkRectF &oldBounds, const PkRectF &newBounds, bool calcInsets = true) {
    KoShapeStrokeSP s = pkSharedPointerDynamicCast<KoShapeStroke>(stroke);
    if (s) {
        PkBrush b = s->lineBrush();
        if (b.gradient()) {
            PkRectF nb = newBounds;
            PkRectF ob = oldBounds;
            if (calcInsets) {
                KoInsets insets;
                s->strokeInsets(nullptr, insets);
                nb.adjust(-insets.left, -insets.top, insets.right, insets.bottom);
                ob.adjust(-insets.left, -insets.top, insets.right, insets.bottom);
            }

            PkRectF relative = KisAlgebra2D::absoluteToRelative(ob, nb);
            KoShapeStrokeSP newStroke(new KoShapeStroke(*s.data()));
            PkTransform newTf = PkTransform::fromTranslate(relative.x(), relative.y());
            newTf.scale(relative.width(), relative.height());
            PkBrush newBrush(*cloneAndTransformGradient(b.gradient(), newTf));
            newStroke->setLineBrush(newBrush);
            return newStroke;
        }
    }
    return stroke;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
KoShape *
KoSvgTextShape::Private::collectPaths(const KoSvgTextShape *rootShape, PkVector<CharacterResult> &result, int &currentIndex)
{

    PkList<KoShape *> shapes;

    if (!internalShapes().isEmpty()) {
        PkList<KoShape *> internalS = internalShapes();
        std::sort(internalS.begin(), internalS.end(), KoShape::compareShapeZIndex);
        Q_FOREACH(KoShape *shape, internalS) {
            KoShape *clone = shape->cloneShape();
            clone->setZIndex(shapes.size());
            if (clone->paintOrder() != rootShape->paintOrder()) {
                clone->setInheritPaintOrder(false);
            }
            shapes.append(clone);
        }
    }

    KoShapeFactoryBase *imageFactory = KoShapeRegistry::instance()->value("ImageShape");
    const PkString imageProp = "image";
    const PkString imageViewTransformProp = "viewboxTransform";
    KoShapeFactoryBase *rectangleFactory = KoShapeRegistry::instance()->value("RectangleShape");

    KoShapeStrokeModelSP stroke = rootShape->stroke();
    PkSharedPointer<KoShapeBackground> background = rootShape->background();
    PkVector<KoShape::PaintOrder> paintOrder = rootShape->paintOrder();

    const PkRectF rootOutline = Private::boundingBoxFromTree(textData, rootShape, false);

    bool currentNodeInheritsBg = false;
    bool currentNodeInheritsStroke = false;

    for (auto it = compositionBegin(textData); it != compositionEnd(textData); it++) {
        bool hasPaintOrder = it->properties.hasProperty(KoSvgTextProperties::PaintOrder);
        inheritPaintProperties(it, stroke, background, paintOrder);
        PkMap<KoSvgText::TextDecoration, PkPainterPath> textDecorations = it->textDecorations;
        PkColor textDecorationColor = it->properties.propertyOrDefault(KoSvgTextProperties::TextDecorationColorId).value<PkColor>();
        PkSharedPointer<KoShapeBackground> decorationColor = background;
        if (textDecorationColor.isValid() && it->properties.hasProperty(KoSvgTextProperties::TextDecorationColorId)) {
            decorationColor = PkSharedPointer<KoColorBackground>(new KoColorBackground(textDecorationColor));
        }
        KoInsets insets;
        if (stroke) {
            stroke->strokeInsets(rootShape, insets);
        }

        KoShape::PaintOrder first = paintOrder.at(0);
        KoShape::PaintOrder second = paintOrder.at(1);

        if (it != compositionBegin(textData)) {
            currentNodeInheritsBg = currentNodeInheritsBg? !it->properties.hasProperty(KoSvgTextProperties::FillId): false;
            currentNodeInheritsStroke = currentNodeInheritsStroke? !it->properties.hasProperty(KoSvgTextProperties::StrokeId): false;
        }

        if (it.state() == KisForestDetail::Enter) {


            if (textDecorations.contains(KoSvgText::DecorationUnderline)) {
                KoPathShape *shape = KoPathShape::createShapeFromPainterPath(textDecorations.value(KoSvgText::DecorationUnderline));
                shape->setBackground(transformBackgroundToBounds(decorationColor,
                                                                 rootOutline,
                                                                 shape->outlineRect()));
                shape->setStroke(stroke);
                shape->setZIndex(shapes.size());
                shape->setFillRule(Pk::WindingFill);
                if (hasPaintOrder)
                    shape->setPaintOrder(first, second);
                shapes.append(shape);
                if (currentNodeInheritsBg && !textDecorationColor.isValid()) {
                    shape->setInheritBackground(true);
                }
                shape->setInheritStroke(currentNodeInheritsStroke);
            }
            if (textDecorations.contains(KoSvgText::DecorationOverline)) {
                KoPathShape *shape = KoPathShape::createShapeFromPainterPath(textDecorations.value(KoSvgText::DecorationOverline));
                shape->setBackground(transformBackgroundToBounds(decorationColor,
                                                                 rootOutline,
                                                                 shape->outlineRect()));
                shape->setStroke(transformStrokeBgToNewBounds(stroke,
                                                              rootOutline,
                                                              shape->outlineRect()));
                shape->setZIndex(shapes.size());
                shape->setFillRule(Pk::WindingFill);
                if (hasPaintOrder)
                    shape->setPaintOrder(first, second);
                shapes.append(shape);
                if (currentNodeInheritsBg && !textDecorationColor.isValid()) {
                    shape->setInheritBackground(true);
                }
                shape->setInheritStroke(currentNodeInheritsStroke);
            }

            if (childCount(siblingCurrent(it)) == 0) {
                PkPainterPath chunk;

                const int j = it->finalResultIndex;
                if (j < 0 || j > result.size()) continue;
                for (int i = currentIndex; i < j; i++) {
                    if (result.at(i).addressable && !result.at(i).hidden) {
                        const PkTransform tf = result.at(i).finalTransform();
                        if (const auto *colorGlyph = std::get_if<Glyph::ColorLayers>(&result.at(i).glyph)) {
                            for (int c = 0; c < colorGlyph->paths.size(); c++) {
                                PkBrush color = colorGlyph->colors.at(c);
                                bool replace = colorGlyph->replaceWithForeGroundColor.at(c);
                                // In theory we can use the pattern or gradient as well
                                // for ColorV0 fonts, but ColorV1 fonts can have
                                // gradients, so I am hesitant.
                                KoColorBackground *b = dynamic_cast<KoColorBackground *>(background.data());
                                if (b && replace) {
                                    color = b->brush();
                                }
                                KoPathShape *shape = KoPathShape::createShapeFromPainterPath(tf.map(colorGlyph->paths.at(c)));
                                shape->setBackground(PkSharedPointer<KoColorBackground>(new KoColorBackground(color.color())));
                                shape->setZIndex(shapes.size());
                                shape->setFillRule(Pk::WindingFill);
                                shape->setPaintOrder(first, second);
                                shapes.append(shape);
                                if (replace && currentNodeInheritsBg) {
                                    shape->setInheritBackground(true);
                                }
                                shape->setInheritStroke(currentNodeInheritsStroke);
                            }
                        } else if (const auto *outlineGlyph = std::get_if<Glyph::Outline>(&result.at(i).glyph)) {
                            chunk.addPath(tf.map(outlineGlyph->path));
                        } else if (const auto *bitmapGlyph = std::get_if<Glyph::Bitmap>(&result.at(i).glyph)) {

                            for (int b = 0; b < bitmapGlyph->images.size(); b++) {
                                PkImage img = bitmapGlyph->images.at(b);
                                PkRectF drawRect = bitmapGlyph->drawRects.at(b);
                                KoProperties params;
                                PkTransform imageTf = PkTransform::fromTranslate(drawRect.x(), drawRect.y());
                                PkTransform viewBox = PkTransform::fromScale(drawRect.width()/img.width(),
                                                                           drawRect.height()/img.height());
                                params.setProperty(toPkString(imageProp), PkVariant::fromValue(img));
                                params.setProperty(toPkString(imageViewTransformProp), PkVariant::fromValue(viewBox));
                                KoShape *shape = imageFactory->createShape(&params);
                                if (img.format() == PkImage::Format_Grayscale8 || img.format() == PkImage::Format_Mono) {
                                    KoShape *rect = rectangleFactory->createDefaultShape();
                                    shape->setSize(drawRect.size());
                                    rect->setSize(drawRect.size());
                                    rect->setStroke(nullptr);
                                    KoClipMask *mask = new KoClipMask();
                                    mask->setShapes({shape});
                                    rect->setClipMask(mask);
                                    rect->setZIndex(shapes.size());
                                    rect->setBackground(transformBackgroundToBounds(background,
                                                                                    rootShape->outlineRect(),
                                                                                    tf.mapRect(drawRect)));
                                    rect->setTransformation(imageTf*tf);

                                    shapes.append(rect);
                                    rect->setInheritBackground(currentNodeInheritsBg);
                                    rect->setInheritStroke(currentNodeInheritsStroke);
                                } else {
                                    shape->setSize(drawRect.size());
                                    shape->setTransformation(imageTf*tf);
                                    shape->setZIndex(shapes.size());
                                    shapes.append(shape);
                                }
                            }
                        }
                    }
                }
                KoPathShape *shape = KoPathShape::createShapeFromPainterPath(chunk);
                shape->setBackground(transformBackgroundToBounds(background,
                                                                 rootOutline,
                                                                 shape->outlineRect()));

                shape->setStroke(transformStrokeBgToNewBounds(stroke,
                                                              rootOutline,
                                                              shape->outlineRect()));
                shape->setZIndex(shapes.size());
                shape->setFillRule(Pk::WindingFill);
                if (hasPaintOrder)
                    shape->setPaintOrder(first, second);
                shapes.append(shape);
                shape->setInheritBackground(currentNodeInheritsBg);
                shape->setInheritStroke(currentNodeInheritsStroke);
                currentIndex = j;

            }
            if (it == compositionBegin(textData)) {
                // We don't want the root to inherit stroke or fill, but after
                // that it should, so we turn both to inherit at the end of the
                // 'enter' code block for the root.
                currentNodeInheritsBg = true;
                currentNodeInheritsStroke = true;
            }
        }
        if (it.state() ==KisForestDetail::Leave) {
            inheritPaintProperties(it, stroke, background, paintOrder);
            if (textDecorations.contains(KoSvgText::DecorationLineThrough)) {
                KoPathShape *shape = KoPathShape::createShapeFromPainterPath(textDecorations.value(KoSvgText::DecorationLineThrough));
                shape->setBackground(transformBackgroundToBounds(decorationColor,
                                                                 rootOutline,
                                                                 shape->outlineRect()));
                shape->setStroke(transformStrokeBgToNewBounds(stroke,
                                                              rootOutline,
                                                              shape->outlineRect()));
                shape->setZIndex(shapes.size());
                shape->setFillRule(Pk::WindingFill);
                if (hasPaintOrder)
                    shape->setPaintOrder(first, second);
                shapes.append(shape);
                if (currentNodeInheritsBg && !textDecorationColor.isValid()) {
                    shape->setInheritBackground(true);
                }
                shape->setInheritStroke(currentNodeInheritsStroke);
            }
        }
    }

    KoShape *parentShape = new KoPathShape();
    if (shapes.size() == 1) {
        parentShape = shapes.first();
    } else if (shapes.size() > 1) {
        KoShapeGroup *group = new KoShapeGroup();
        KoShapeGroupCommand cmd(group, toPkList(shapes), false);
        cmd.redo();

        group->setBackground(rootShape->background());
        group->setStroke(rootShape->stroke());
        group->setPaintOrder(rootShape->paintOrder().first(), rootShape->paintOrder().at(1));
        group->setInheritPaintOrder(rootShape->inheritPaintOrder());

        parentShape = group;
    }
    parentShape->setZIndex(rootShape->zIndex());
    parentShape->setTransformation(rootShape->absoluteTransformation());
    parentShape->setName(rootShape->name());

    return parentShape;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void KoSvgTextShape::Private::paintDebug(PkPainter &painter,
                                         const PkVector<CharacterResult> &result,
                                         int &currentIndex)
{

    for (auto it = textData.depthFirstTailBegin(); it != textData.depthFirstTailEnd(); it++) {
        const int j = it->finalResultIndex;
        if (currentIndex > result.size()) continue;
        if (j < 0 || j > result.size()) continue;

        const PkRect shapeGlobalClipRect = painter.transform().mapRect(it->associatedOutline.boundingRect()).toAlignedRect();

        painter.save();

        if (shapeGlobalClipRect.isValid() && childCount(siblingCurrent(it)) == 0) {
            for (int i = currentIndex; i < j; i++) {
                if (result.at(i).addressable && !result.at(i).hidden) {
                    const PkTransform tf = result.at(i).finalTransform();

#if 1 // Debug: draw character bounding boxes
                    painter.setBrush(PkBrush(Pk::NoBrush));
                    PkPen pen(PkColor(0, 0, 0, 50));
                    pen.setCosmetic(true);
                    pen.setWidth(2);
                    painter.setPen(pen);
                    if (const auto *bitmapGlyph = std::get_if<Glyph::Bitmap>(&result.at(i).glyph)) {
                        Q_FOREACH(const PkRectF drawRect, bitmapGlyph->drawRects) {
                            painter.drawPolygon(tf.map(drawRect));
                        }
                    } else if (const auto *colorGlyph = std::get_if<Glyph::ColorLayers>(&result.at(i).glyph)) {
                        PkRectF boundingRect;
                        Q_FOREACH (const PkPainterPath &p, colorGlyph->paths) {
                            boundingRect |= p.boundingRect();
                        }
                        painter.drawPolygon(tf.map(boundingRect));
                    } else if (const auto *outlineGlyph = std::get_if<Glyph::Outline>(&result.at(i).glyph)) {
                        painter.drawPolygon(tf.map(outlineGlyph->path.boundingRect()));
                    }
                    PkColor penColor = result.at(i).anchored_chunk ? result.at(i).isHanging ? PkColor(Pk::red) : PkColor(Pk::magenta)
                        : result.at(i).lineEnd == LineEdgeBehaviour::NoChange ? PkColor(Pk::cyan)
                                                                              : PkColor(Pk::yellow);
                    penColor.setAlpha(192);
                    pen.setColor(penColor);
                    painter.setPen(pen);
                    painter.drawPolygon(tf.map(result.at(i).layoutBox()));

                    penColor.setAlpha(96);
                    pen.setColor(penColor);
                    pen.setWidth(1);
                    pen.setStyle(Pk::DotLine);
                    painter.setPen(pen);
                    painter.drawPolygon(tf.map(result.at(i).lineHeightBox()));

                    pen.setStyle(Pk::SolidLine);
                    pen.setWidth(2);

                    penColor.setAlpha(192);
                    pen.setColor(penColor);
                    painter.setPen(pen);
                    painter.drawLine(tf.map(result.at(i).cursorInfo.caret));


                    const PkPointF center = tf.mapRect(result.at(i).layoutBox()).center();
                    PkString text = "#";
                    text += PkString::number(i);
                    {
                        // Find the range of this typographic character
                        int end = i + 1;
                        while (end < result.size() && result[end].middle) {
                            end++;
                        }
                        end--;
                        if (end != i) {
                            text += "~";
                            text += PkString::number(end);
                        }
                    }
                    text += PkString("\n(%1)").arg(result.at(i).plaintTextIndex);
                    const PkPointF viewCenter = painter.transform().map(center);
                    painter.save();
                    painter.setTransform(PkTransform());
                    painter.setPen(PkPen(Pk::red));
                    painter.drawText(viewCenter, text);
                    painter.restore();

                    pen.setWidth(6);
                    const BreakType breakType = result.at(i).breakType;
                    if (breakType == BreakType::SoftBreak || breakType == BreakType::HardBreak) {
                        if (breakType == BreakType::SoftBreak) {
                            penColor = PkColor(Pk::blue);
                        } else if (breakType == BreakType::HardBreak) {
                            penColor = PkColor(Pk::red);
                        }
                        penColor.setAlpha(128);
                        pen.setColor(penColor);
                        painter.setPen(pen);
                        painter.drawPoint(center);
                    }
                    //ligature carets
                    penColor = PkColor(Pk::darkGreen);
                    penColor.setAlpha(192);
                    pen.setColor(penColor);
                    painter.setPen(pen);
                    PkVector<PkPointF> offset = result.at(i).cursorInfo.offsets;
                    for (int k=0; k<offset.size(); k++) {
                        painter.drawPoint(tf.map(offset.at(k)));
                    }
                    // Finalpos
                    penColor = PkColor(Pk::red);
                    penColor.setAlpha(192);
                    pen.setColor(penColor);
                    painter.setPen(pen);
                    painter.drawPoint(result.at(i).finalPosition);
#endif
                }
            }
        }
        painter.restore();
        currentIndex = j;
    }
}
