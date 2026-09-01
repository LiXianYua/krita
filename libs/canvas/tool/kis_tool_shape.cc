/*
 *  SPDX-FileCopyrightText: 2005 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_shape.h"

#include <KoUnit.h>
#include <KoShape.h>
#include <KoGradientBackground.h>
#include <KoCanvasBase.h>
#include <KoShapeController.h>
#include <KoColorBackground.h>
#include <KoPatternBackground.h>
#include <KoShapeStroke.h>
#include <KoDocumentResourceManager.h>
#include <KoPathShape.h>
#include <PkFlakeBridge.h>

#include <klocalizedstring.h>
#include <ksharedconfig.h>

#include <kis_debug.h>
#include <brushengine/kis_paintop_registry.h>
#include <kis_paint_layer.h>
#include <kis_paint_device.h>
#include "kis_figure_painting_tool_helper.h"
#include <kis_node_query_path.h>

#include <KoSelectedShapesProxy.h>
#include <KoSelection.h>
#include <commands/KoKeepShapesSelectedCommand.h>
#include "kis_selection_mask.h"
#include <KisShapeSelectionMarker.h>
#include "kis_processing_applicator.h"


KisToolShape::KisToolShape(KoCanvasBase * canvas, const QCursor & cursor)
        : KisToolPaint(canvas, cursor)
{
}

KisToolShape::~KisToolShape()
{
}

void KisToolShape::activate(const QSet<KoShape*> &shapes)
{
    KisToolPaint::activate(shapes);
    m_configGroup =  KSharedConfig::openConfig()->group(toolId());
}


int KisToolShape::flags() const
{
    return KisTool::FLAG_USES_CUSTOM_COMPOSITEOP|KisTool::FLAG_USES_CUSTOM_PRESET
           |KisTool::FLAG_USES_CUSTOM_SIZE;
}

QWidget * KisToolShape::createOptionWidget()
{
    return nullptr;
}

KisToolShapeUtils::FillStyle KisToolShape::fillStyle()
{
    return static_cast<KisToolShapeUtils::FillStyle>(
        m_configGroup.readEntry("fillType", int(KisToolShapeUtils::FillStyleNone)));
}

KisToolShapeUtils::StrokeStyle KisToolShape::strokeStyle()
{
    const auto fillStyle = this->fillStyle();
    auto strokeStyle = static_cast<KisToolShapeUtils::StrokeStyle>(
        m_configGroup.readEntry("outlineType", int(KisToolShapeUtils::StrokeStyleNone)));

    if (fillStyle == KisToolShapeUtils::FillStyleNone
        && strokeStyle == KisToolShapeUtils::StrokeStyleNone) {
        strokeStyle = KisToolShapeUtils::StrokeStyleForeground;
    }

    return strokeStyle;
}

QTransform KisToolShape::fillTransform()
{
    QTransform transform;

    transform.rotate(m_configGroup.readEntry("patternTransformRotation", 0));
    const qreal scale = m_configGroup.readEntry("patternTransformScale", 100) * 0.01;
    transform.scale(scale, scale);

    return transform;
}

qreal KisToolShape::currentStrokeWidth() const
{
    const qreal sizeInPx =
        canvas()->resourceManager()->resource(KoCanvasResource::Size).toReal();

    return canvas()->unit().fromUserValue(sizeInPx);
}

KisToolShape::ShapeAddInfo KisToolShape::shouldAddShape(KisNodeSP currentNode) const
{
    ShapeAddInfo info;

    if (currentNode->inherits("KisShapeLayer")) {
        info.shouldAddShape = true;
    } else if (KisSelectionMask *mask = dynamic_cast<KisSelectionMask*>(currentNode.data())) {
        if (mask->selection()->hasShapeSelection()) {
            info.shouldAddShape = true;
            info.shouldAddSelectionShape = true;
        }
    }

    return info;
}

void KisToolShape::ShapeAddInfo::markAsSelectionShapeIfNeeded(KoShape *shape) const
{
    if (this->shouldAddSelectionShape) {
        shape->setUserData(new KisShapeSelectionMarker());
    }
}

void KisToolShape::addShape(KoShape* shape)
{
    using namespace KisToolShapeUtils;

    KisResourcesSnapshot resources(image(),
                                   currentNode(),
                                   canvas()->resourceManager()->canvasResourcesInterface());
    switch(fillStyle()) {
        case FillStyleForegroundColor:
            shape->setBackground(QSharedPointer<KoColorBackground>(new KoColorBackground(toQColor(resources.currentFgColor().toQColor()))));
            break;
        case FillStyleBackgroundColor:
            shape->setBackground(QSharedPointer<KoColorBackground>(new KoColorBackground(toQColor(resources.currentBgColor().toQColor()))));
            break;
        case FillStylePattern:
            shape->setBackground(QSharedPointer<KoShapeBackground>(0));
            break;
        case FillStyleNone:
            shape->setBackground(QSharedPointer<KoShapeBackground>(0));
            break;
    }

    switch (strokeStyle()) {
    case KisToolShapeUtils::StrokeStyleNone:
        shape->setStroke(KoShapeStrokeModelSP());
        break;
    case KisToolShapeUtils::StrokeStyleForeground:
    case KisToolShapeUtils::StrokeStyleBackground: {
        KoShapeStrokeSP stroke(new KoShapeStroke());
        stroke->setLineWidth(currentStrokeWidth());
        const QColor color = strokeStyle() == KisToolShapeUtils::StrokeStyleForeground ?
                    toQColor(resources.currentFgColor().toQColor()) :
                    toQColor(resources.currentBgColor().toQColor());
        stroke->setColor(color);
        shape->setStroke(stroke);
        break;
    }
    }

    KUndo2Command *parentCommand = new KUndo2Command();

    KoSelection *selection = canvas()->selectedShapesProxy()->selection();
    const QList<KoShape*> oldSelectedShapes = selection->selectedShapes();

    // reset selection on the newly added shape :)
    // TODO: think about moving this into controller->addShape?
    new KoKeepShapesSelectedCommand(toPkList(oldSelectedShapes), PkList<KoShape*>{shape}, canvas()->selectedShapesProxy(), false, parentCommand);
    KUndo2Command *cmd = canvas()->shapeController()->addShape(shape, 0, parentCommand);
    parentCommand->setText(cmd->text());
    new KoKeepShapesSelectedCommand(toPkList(oldSelectedShapes), PkList<KoShape*>{shape}, canvas()->selectedShapesProxy(), true, parentCommand);

    KisProcessingApplicator::runSingleCommandStroke(image(), cmd, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::EXCLUSIVE);
}

void KisToolShape::addPathShape(KoPathShape* pathShape, const KUndo2MagicString& name)
{
    KisNodeSP node = currentNode();
    if (!node) {
        return;
    }

    // Compute the outline
    KisImageSP image = this->image();
    QTransform matrix;
    matrix.scale(image->xRes(), image->yRes());
    matrix.translate(pathShape->position().x(), pathShape->position().y());
    QPainterPath mappedOutline = matrix.map(pathShape->outline());

    if (node->hasEditablePaintDevice()) {
        KisFigurePaintingToolHelper helper(name,
                                           image,
                                           node,
                                           canvas()->resourceManager()->canvasResourcesInterface(),
                                           strokeStyle(),
                                           fillStyle(),
                                           toPkTransform(fillTransform()));
        helper.paintPainterPath(toPkPainterPath(mappedOutline));
    } else if (node->inherits("KisShapeLayer")) {
        pathShape->normalize();
        addShape(pathShape);

    }
}
