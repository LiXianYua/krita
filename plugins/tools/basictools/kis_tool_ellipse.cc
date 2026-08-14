/*
 *  kis_tool_ellipse.cc - part of Krayon
 *
 *  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@compuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2004 Clarence Dang <dang@kde.org>
 *  SPDX-FileCopyrightText: 2009 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_ellipse.h"
#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoShapeStroke.h>
#include <KisCanvasToolServices.h>

#include <kis_shape_tool_helper.h>
#include "kis_figure_painting_tool_helper.h"
#include <brushengine/kis_paintop_preset.h>

KisToolEllipse::KisToolEllipse(KoCanvasBase * canvas)
        : KisToolEllipseBase(canvas, KisToolEllipseBase::PAINT, dynamic_cast<KisCanvasToolServices *>(canvas)->toolLoadCursor("tool_ellipse_cursor.png", 6, 6))
{
    setObjectName("tool_ellipse");
    setSupportOutline(true);
    setIsOpacityPresetMode(true);

    connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const QVariant &) {
                if (key == KoCanvasResource::CurrentEffectiveCompositeOp) {
                    resetCursorStyle();
                }
            });
}

KisToolEllipse::~KisToolEllipse()
{
}

void KisToolEllipse::resetCursorStyle()
{
    if (isEraser() && (nodePaintAbility() == NodePaintAbility::PAINT)) {
        useCursor(dynamic_cast<KisCanvasToolServices *>(canvas())->toolLoadCursor("tool_ellipse_eraser_cursor.png", 6, 6));
    } else {
        KisToolEllipseBase::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisToolEllipse::finishRect(const QRectF& rect, qreal roundCornersX, qreal roundCornersY)
{
    Q_UNUSED(roundCornersX);
    Q_UNUSED(roundCornersY);

    if (rect.isEmpty())
        return;

    const KisToolShape::ShapeAddInfo info =
        shouldAddShape(currentNode());

    if (!info.shouldAddShape) {
        KisFigurePaintingToolHelper helper(kundo2_i18n("Draw Ellipse"),
                                           image(),
                                           currentNode(),
                                           canvas()->resourceManager()->canvasResourcesInterface(),
                                           strokeStyle(),
                                           fillStyle(),
                                           fillTransform());
        QPainterPath path;
        path.addEllipse(rect);
        getRotatedPath(path, rect.center(), getRotationAngle());
        helper.paintPainterPath(path);
    } else {
        KisResourcesSnapshot resources(image(),
                                       currentNode(),
                                       canvas()->resourceManager()->canvasResourcesInterface());
        QRectF r = convertToPt(rect);
        KoShape* shape = KisShapeToolHelper::createEllipseShape(r);
        shape->rotate(qRadiansToDegrees(getRotationAngle()));
        KoShapeStrokeSP border(new KoShapeStroke(currentStrokeWidth(), resources.currentFgColor().toQColor()));
        shape->setStroke(border);

        info.markAsSelectionShapeIfNeeded(shape);

        addShape(shape);
    }
}

bool KisToolEllipse::supportsPaintingAssistants() const
{
    return true;
}
