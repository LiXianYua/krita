/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2007-2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ShapeRotateStrategy.h"
#include "SelectionDecorator.h"
#include "DefaultToolStrategyMath.h"

#include <KoToolBase.h>
#include <KoCanvasBase.h>
#include <KoSelection.h>
#include <KoPointerEvent.h>
#include <KoShapeManager.h>
#include <KoCanvasResourceProvider.h>
#include <commands/KoShapeTransformCommand.h>
#include <KoShapeBulkActionLock.h>

#include <PkPoint.h>
#include <math.h>
#include <klocalizedstring.h>

ShapeRotateStrategy::ShapeRotateStrategy(KoToolBase *tool, KoSelection *selection, const PkPointF &clicked, Qt::MouseButtons buttons)
    : KoInteractionStrategy(tool)
    , m_start(clicked)
{
    /**
     * The outline of the selection should look as if it is also rotated, so we
     * add it to the transformed shapes list.
     */
    m_transformedShapesAndSelection = selection->selectedEditableShapes();
    m_transformedShapesAndSelection << selection;

    for (KoShape *shape : m_transformedShapesAndSelection) {
        m_oldTransforms << shape->transformation();
    }

    KoFlake::AnchorPosition anchor = !(buttons & Qt::RightButton) ?
                KoFlake::Center :
                KoFlake::AnchorPosition(tool->canvas()->resourceManager()->resource(KoFlake::HotPosition).toInt());

    m_rotationCenter = selection->absolutePosition(anchor);

    tool->setStatusText(PkString("Press ALT to rotate in 45 degree steps."));
}

void ShapeRotateStrategy::handleMouseMove(const PkPointF &point, Qt::KeyboardModifiers modifiers)
{
    qreal angle = atan2(point.y() - m_rotationCenter.y(), point.x() - m_rotationCenter.x()) -
                  atan2(m_start.y() - m_rotationCenter.y(), m_start.x() - m_rotationCenter.x());
    angle = angle / M_PI * 180;  // convert to degrees.
    angle = DefaultToolStrategyMath::snappedRotationDegrees(
        angle, modifiers & (Qt::AltModifier | Qt::ControlModifier));

    rotateBy(angle);
}

void ShapeRotateStrategy::rotateBy(qreal angle)
{
    PkTransform matrix;
    matrix.translate(m_rotationCenter.x(), m_rotationCenter.y());
    matrix.rotate(angle);
    matrix.translate(-m_rotationCenter.x(), -m_rotationCenter.y());

    PkTransform applyMatrix = matrix * m_rotationMatrix.inverted();
    m_rotationMatrix = matrix;

    KoShapeBulkActionLock lock(m_transformedShapesAndSelection);

    for (KoShape *shape : m_transformedShapesAndSelection) {
        shape->applyAbsoluteTransformation(applyMatrix);
    }

    KoShapeBulkActionLock::bulkShapesUpdate(lock.unlock());
}

void ShapeRotateStrategy::paint(PkPainter &painter, const KoViewConverter &converter)
{
    // paint the rotation center
    painter.setPen(PkPen(Qt::red));
    painter.setBrush(PkBrush(Qt::red));
    painter.setRenderHint(PkPainter::Antialiasing, true);
    PkRectF circle(0, 0, handleRadius(), handleRadius());
    circle.moveCenter(converter.documentToView(m_rotationCenter));
    painter.drawEllipse(circle);
}

KUndo2Command *ShapeRotateStrategy::createCommand()
{
    PkList<PkTransform> newTransforms;
    for (KoShape *shape : m_transformedShapesAndSelection) {
        newTransforms << shape->transformation();
    }

    KoShapeTransformCommand *cmd = new KoShapeTransformCommand(m_transformedShapesAndSelection, m_oldTransforms, newTransforms);
    cmd->setText(kundo2_i18n("Rotate"));
    return cmd;
}
