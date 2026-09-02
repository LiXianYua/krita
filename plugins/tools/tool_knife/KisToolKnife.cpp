/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtWidgets/QtWidgets>
#include <QtXml/QtXml>
#include <PkFlakeBridge.h>
#include "KisToolKnife.h"

#include <PkBrush.h>
#include <PkPainter.h>
#include <PkPainterPath.h>

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

Q_DECLARE_METATYPE(KisNodeWSP)


struct KisToolKnife::Private {
    PkPointF startPoint = PkPointF(0, 0);
    PkPointF endPoint = PkPointF(0, 0);
    PkRectF previousLineDirtyRect = PkRectF();
};


KisToolKnife::KisToolKnife(KoCanvasBase * canvas)
    : KoInteractionTool(canvas),
      m_d(new Private)
{
    setObjectName("tool_knife");
    useCursor(QCursor(Qt::ArrowCursor));
    repaintDecorations();
}

KisToolKnife::~KisToolKnife()
{
}

void KisToolKnife::paint(QPainter &painter, const KoViewConverter &converter)
{
    KoInteractionTool::paint(painter, converter);
}

void KisToolKnife::activate(const QSet<KoShape *> &shapes)
{
    KoInteractionTool::activate(shapes);
        useCursor(QCursor(Qt::ArrowCursor));

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
                toQString(PkString("This tool only works on vector layers. You probably want to create a vector layer and a starting shape first.")),
                {}, 2000, KisCanvasFeedback::Priority::Medium, Qt::AlignCenter);
        return;
    }

    KoInteractionTool::mousePressEvent(event);
}

void KisToolKnife::mouseMoveEvent(KoPointerEvent *event)
{
    KoInteractionTool::mouseMoveEvent(event);

    if (event->buttons().testFlag(Qt::MouseButton::LeftButton)) {

        m_d->endPoint = toPkPointF(event->point);
        PkRectF dirtyRect;
        KisAlgebra2D::accumulateBounds(m_d->startPoint, &dirtyRect);
        KisAlgebra2D::accumulateBounds(m_d->endPoint, &dirtyRect);

        PkRectF accumulatedWithPrevious = m_d->previousLineDirtyRect;
        accumulatedWithPrevious |= dirtyRect;

        canvas()->updateCanvas(toQRectF(accumulatedWithPrevious));
        m_d->previousLineDirtyRect = dirtyRect;

    }

    repaintDecorations();
}

void KisToolKnife::mouseReleaseEvent(KoPointerEvent *event)
{
    KoInteractionTool::mouseReleaseEvent(event);

    m_d->endPoint = toPkPointF(event->point);

    PkRectF dirtyRect;
    KisAlgebra2D::accumulateBounds(m_d->startPoint, &dirtyRect);
    KisAlgebra2D::accumulateBounds(m_d->endPoint, &dirtyRect);

    PkRectF accumulatedWithPrevious = m_d->previousLineDirtyRect | dirtyRect;

    canvas()->updateCanvas(toQRectF(accumulatedWithPrevious));
    m_d->previousLineDirtyRect = dirtyRect;
}

namespace {

// Mirrors the deleted KisToolKnifeOptionsWidget::Private::widthTypeFromString():
// same four config strings, same "thick" fallback for anything else.
enum class GutterWidthType { Thick, Thin, Special, Automatic };

GutterWidthType gutterWidthTypeFromConfigString(const PkString &type)
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
    PkList<KoShape*> shapes = toPkList(canvas()->shapeManager()->shapes());

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
    PkString unitSymbol = toPkString(configGroup.readEntry("gutter_unit_symbol", "px"));
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
        gutterWidthTypeFromConfigString(toPkString(configGroup.readEntry("current_gutter_width_type", "thick")));

    GutterWidthsConfig widthsConfig = [&]() {
        if (currentWidthType == GutterWidthType::Automatic) {
            const GutterWidthType horizontalType =
                gutterWidthTypeFromConfigString(toPkString(configGroup.readEntry("automatic_horizontal_type", "thick")));
            const GutterWidthType verticalType =
                gutterWidthTypeFromConfigString(toPkString(configGroup.readEntry("automatic_vertical_type", "thin")));
            const GutterWidthType diagonalType =
                gutterWidthTypeFromConfigString(toPkString(configGroup.readEntry("automatic_diagonal_type", "thin")));
            return GutterWidthsConfig(unit, resolution,
                                       widthForType(horizontalType),
                                       widthForType(verticalType),
                                       widthForType(diagonalType),
                                       gutterAngleDegrees);
        }
        return GutterWidthsConfig(unit, resolution, widthForType(currentWidthType), gutterAngleDegrees);
    }();

    return new CutThroughShapeStrategy(this, canvas()->selectedShapesProxy()->selection(), shapes,
                                       toPkPointF(event->point), widthsConfig);
}

bool KisToolKnife::isValidForCurrentLayer() const
{
    KisNodeSP node = canvas()->resourceManager()
                            ->resource(KoCanvasResource::CurrentKritaNode)
                            .value<KisNodeWSP>();
    const KisShapeLayer *shapeLayer = dynamic_cast<const KisShapeLayer*>(node.data());
    return (shapeLayer != nullptr);
}

KoToolBase *KisToolKnifeFactory::createTool(KoCanvasBase *canvas)
{
    return new KisToolKnife(canvas);
}
