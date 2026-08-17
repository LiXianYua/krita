/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisToolKnife.h"

#include "QApplication"
#include "QPainterPath"

#include <klocalizedstring.h>
#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoColor.h>
#include <KoPointerEvent.h>
#include <KoViewConverter.h>
#include <KisCanvasFeedback.h>
#include <kis_image.h>
#include <kis_shape_controller.h>
#include "kis_painter.h"
#include "kis_paintop_preset.h"
#include "kis_shape_layer.h"

#include "kundo2magicstring.h"
#include "kundo2stack.h"
#include "commands_new/kis_transaction_based_command.h"
#include "kis_transaction.h"
#include "KoPathShape.h"
#include <KoShapeController.h>

#include "kis_processing_applicator.h"
#include "kis_datamanager.h"
#include "KoColorSpaceRegistry.h"
#include <KisCursorOverrideLock.h>

#include "libs/image/kis_paint_device_debug_utils.h"

#include <KoSelectedShapesProxy.h>

#include "CutThroughShapeStrategy.h"

#include "kis_paint_layer.h"
#include "kis_algebra_2d.h"
#include "kis_resources_snapshot.h"
#include <KoSelection.h>
#include <KoShapeManager.h>
#include <KoUnit.h>
#include <ksharedconfig.h>


struct KisToolKnife::Private {
    QPointF startPoint = QPointF(0, 0);
    QPointF endPoint = QPointF(0, 0);
    QRectF previousLineDirtyRect = QRectF();
};


KisToolKnife::KisToolKnife(KoCanvasBase * canvas)
    : KoInteractionTool(canvas),
      m_d(new Private)
{
    setObjectName("tool_knife");
    useCursor(Qt::ArrowCursor);
    repaintDecorations();
}

KisToolKnife::~KisToolKnife()
{
}

void paintSelectedEdge(QPainter &painter, const KoViewConverter &converter, const QLineF &lineSegment)
{
    QLineF lineInView = converter.documentToView().map(lineSegment);
    QList<QLineF> parallels = KisAlgebra2D::getParallelLines(lineInView, 5);

    painter.save();
    qreal width = 2;
    QColor color = Qt::blue;
    color.setAlphaF(0.8);
    QColor white = Qt::white;
    white.setAlphaF(0.75);

    QPen pen = QPen(color, width);
    QPen alternative = QPen(white, width);

    alternative.setStyle(Qt::CustomDashLine);
    qreal dashLength = 6;
    alternative.setDashPattern({dashLength - 1, dashLength + 1});
    alternative.setCapStyle(Qt::RoundCap);

    pen.setCosmetic(true);
    painter.setPen(pen);

    //painter.drawLines(parallels.toVector());
    painter.drawLine(lineInView);

    alternative.setCosmetic(true);
    painter.setPen(alternative);
    //painter.drawLines(parallels.toVector());
    painter.drawLine(lineInView);


    painter.restore();

}

QPolygonF createDiamond(int size, QPointF location = QPointF())
{
    QPolygonF polygon;
    polygon << QPointF(-size, 0);
    polygon << QPointF(0, size);
    polygon << QPointF(size, 0);
    polygon << QPointF(0, -size);
    polygon.translate(location);
    return polygon;
}

void paintSelectedPoint(QPainter &painter, const KoViewConverter &converter, const QPointF &point)
{
    QPointF p = point;
    p = converter.documentToView().map(p);
    QPolygonF diamond = createDiamond(6, p);
    painter.save();
    QColor color = Qt::blue;
    color.setAlphaF(0.9);
    QColor white = Qt::white;
    white.setAlphaF(0.75);

    QPen pen = QPen(color, 2);
    pen.setCosmetic(true);
    QBrush brush = QBrush(white);
    painter.setPen(pen);
    painter.setBrush(brush);

    painter.drawPolygon(diamond);
    painter.restore();
}

void KisToolKnife::paint(QPainter &painter, const KoViewConverter &converter)
{
    Q_UNUSED(converter);

    painter.save();
    painter.restore();

    painter.save();
    painter.setBrush(Qt::darkGray);
    //painter.drawEllipse(converter.documentToView(m_d->mousePoint), 4, 4);

    painter.restore();

    KoInteractionTool::paint(painter, converter);


#ifdef KNIFE_DEBUG
    bool paintSelection = true;
    if (paintSelection) {

        KIS_SAFE_ASSERT_RECOVER_RETURN(canvas());
        KIS_SAFE_ASSERT_RECOVER_RETURN(canvas()->selectedShapesProxy());

        KIS_SAFE_ASSERT_RECOVER_RETURN(canvas()->selectedShapesProxy()->selection());

        KoSelection *selection = canvas()->selectedShapesProxy()->selection();

        QList<KoShape*> shapes = selection->selectedEditableShapes();

        Q_FOREACH(KoShape* shape, shapes) {
            KisAlgebra2D::VectorPath vector = KisAlgebra2D::VectorPath(shape->outline());
            painter.save();
            painter.setTransform(painter.transform());
            for (int i = 0; i < vector.segmentsCount(); i++) {
                paintSelectedEdge(painter, converter, shape->absoluteTransformation().map(vector.segmentAtAsLine(i)));
            }
            for (int i =  0; i < vector.pointsCount(); i++) {
                paintSelectedPoint(painter, converter, shape->absoluteTransformation().map(vector.pointAt(i).endPoint));
            }
            painter.restore();

        }
    }
#endif


}

void KisToolKnife::activate(const QSet<KoShape *> &shapes)
{
    KoInteractionTool::activate(shapes);
    useCursor(Qt::ArrowCursor);

}

void KisToolKnife::deactivate()
{
    KoInteractionTool::deactivate();
}

void KisToolKnife::mousePressEvent(KoPointerEvent *event)
{
    // this tool only works on a vector layer right now, so give a warning if another layer type is trying to use it
    if (!isValidForCurrentLayer()) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback *>(canvas());
        KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
        feedback->showFloatingMessage(
                i18n("This tool only works on vector layers. You probably want to create a vector layer and a starting shape first."),
                QIcon(), 2000, KisCanvasFeedback::Priority::Medium, Qt::AlignCenter);
        return;
    }

    KoInteractionTool::mousePressEvent(event);
}

void KisToolKnife::mouseMoveEvent(KoPointerEvent *event)
{
    KoInteractionTool::mouseMoveEvent(event);

    if (event->buttons().testFlag(Qt::MouseButton::LeftButton)) {

        m_d->endPoint = event->point;
        QRectF dirtyRect;
        KisAlgebra2D::accumulateBounds(m_d->startPoint, &dirtyRect);
        KisAlgebra2D::accumulateBounds(m_d->endPoint, &dirtyRect);

        QRectF accumulatedWithPrevious = m_d->previousLineDirtyRect;
        accumulatedWithPrevious |= dirtyRect;

        canvas()->updateCanvas(accumulatedWithPrevious);
        m_d->previousLineDirtyRect = dirtyRect;

    }

    repaintDecorations();
}

void KisToolKnife::mouseReleaseEvent(KoPointerEvent *event)
{
    KoInteractionTool::mouseReleaseEvent(event);

    m_d->endPoint = event->point;

    QRectF dirtyRect;
    KisAlgebra2D::accumulateBounds(m_d->startPoint, &dirtyRect);
    KisAlgebra2D::accumulateBounds(m_d->endPoint, &dirtyRect);

    QRectF accumulatedWithPrevious = m_d->previousLineDirtyRect | dirtyRect;

    canvas()->updateCanvas(accumulatedWithPrevious);
    m_d->previousLineDirtyRect = dirtyRect;
}

namespace {

// Mirrors the deleted KisToolKnifeOptionsWidget::Private::widthTypeFromString():
// same four config strings, same "thick" fallback for anything else.
enum class GutterWidthType { Thick, Thin, Special, Automatic };

GutterWidthType gutterWidthTypeFromConfigString(const QString &type)
{
    if (type == "thick") {
        return GutterWidthType::Thick;
    } else if (type == "thin") {
        return GutterWidthType::Thin;
    } else if (type == "special") {
        return GutterWidthType::Special;
    } else if (type == "automatic") {
        return GutterWidthType::Automatic;
    }
    return GutterWidthType::Thick;
}

} // namespace

KoInteractionStrategy *KisToolKnife::createStrategy(KoPointerEvent *event)
{
    QList<KoShape*> shapes = canvas()->shapeManager()->shapes();

    KisShapeController *shapeController =
        dynamic_cast<KisShapeController *>(canvas()->shapeController()->documentBase());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shapeController, nullptr);
    qreal resolution = 1.0;
    if (KisImageSP image = shapeController->currentImage()) {
        // we're going to assume isotropic image
        resolution = image->xRes();
    }

    // Options panel removed. This reproduces the panel's default state as it
    // stood at construction (KisToolKnifeOptionsWidget::Private::readFromConfig()
    // + getCurrentWidthsConfig()): mode = AddGutter, and the width/angle read
    // from the same config keys/defaults, dispatched on the same
    // "current_gutter_width_type" the panel used to pick which of
    // thick/thin/special/automatic was active -- so any value the user had
    // already saved (including a non-default width type) is still honoured.
    KConfigGroup configGroup = KSharedConfig::openConfig()->group(toolId());
    QString unitSymbol = configGroup.readEntry("gutter_unit_symbol", "px");
    bool unitConversionSuccess = false;
    KoUnit unit = KoUnit::fromSymbol(unitSymbol, &unitConversionSuccess);
    if (!unitConversionSuccess) {
        unit = KoUnit::fromSymbol("px");
    }

    const qreal thickGutterWidth = configGroup.readEntry("thick_gutter_width", 40.0);
    const qreal thinGutterWidth = configGroup.readEntry("thin_gutter_width", 15.0);
    const qreal specialGutterWidth = configGroup.readEntry("special_gutter_width", 70.0);
    const qreal gutterAngleDegrees = configGroup.readEntry("automatic_gutter_angle", 2.0);

    auto widthForType = [&](GutterWidthType type) -> qreal {
        switch (type) {
        case GutterWidthType::Thick:
            return thickGutterWidth;
        case GutterWidthType::Thin:
            return thinGutterWidth;
        case GutterWidthType::Special:
            return specialGutterWidth;
        default:
            // Matches the panel's getWidthForType(): Automatic (and any
            // other value) falls back to the "special" width here too --
            // Automatic itself is handled per-axis below and never reaches
            // this branch.
            return specialGutterWidth;
        }
    };

    const GutterWidthType currentWidthType =
        gutterWidthTypeFromConfigString(configGroup.readEntry("current_gutter_width_type", "thick"));

    GutterWidthsConfig widthsConfig = [&]() {
        if (currentWidthType == GutterWidthType::Automatic) {
            const GutterWidthType horizontalType =
                gutterWidthTypeFromConfigString(configGroup.readEntry("automatic_horizontal_type", "thick"));
            const GutterWidthType verticalType =
                gutterWidthTypeFromConfigString(configGroup.readEntry("automatic_vertical_type", "thin"));
            const GutterWidthType diagonalType =
                gutterWidthTypeFromConfigString(configGroup.readEntry("automatic_diagonal_type", "thin"));
            return GutterWidthsConfig(unit, resolution,
                                       widthForType(horizontalType),
                                       widthForType(verticalType),
                                       widthForType(diagonalType),
                                       gutterAngleDegrees);
        }
        return GutterWidthsConfig(unit, resolution, widthForType(currentWidthType), gutterAngleDegrees);
    }();

    return new CutThroughShapeStrategy(this, canvas()->selectedShapesProxy()->selection(), shapes, event->point, widthsConfig);
}

bool KisToolKnife::isValidForCurrentLayer() const
{
    KisNodeSP node = canvas()->resourceManager()
                            ->resource(KoCanvasResource::CurrentKritaNode)
                            .value<KisNodeWSP>();
    const KisShapeLayer *shapeLayer = qobject_cast<const KisShapeLayer*>(node.data());
    return (shapeLayer != nullptr);
}
