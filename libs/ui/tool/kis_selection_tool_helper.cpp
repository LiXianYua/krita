/*
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_selection_tool_helper.h"


#include <kundo2command.h>
#include <QTimer>

#include <KoCanvasBase.h>
#include <KoShapeController.h>
#include <KoPathShape.h>

#include "kis_pixel_selection.h"
#include "kis_shape_selection.h"
#include "kis_image.h"
#include "KisSelectionUtils.h"
#include "canvas/kis_coordinates_converter.h"
#include "canvas/KisCanvasFeedback.h"
#include "kis_transaction.h"
#include "commands/kis_selection_commands.h"
#include "kis_shape_controller.h"

#include <kis_icon.h>
#include "kis_processing_applicator.h"
#include "commands_new/kis_transaction_based_command.h"
#include "kis_gui_context_command.h"
#include "kis_command_utils.h"
#include "commands/kis_deselect_global_selection_command.h"

#include "kis_algebra_2d.h"
#include "kis_config.h"


KisSelectionToolHelper::KisSelectionToolHelper(KoCanvasBase *canvas,
                                               KisImageSP image,
                                               KisNodeSP activeNode,
                                               const KUndo2MagicString& name)
        : m_canvas(canvas)
        , m_image(image)
        , m_activeNode(activeNode)
        , m_name(name)
{
}

KisSelectionToolHelper::~KisSelectionToolHelper()
{
}

struct LazyInitGlobalSelection : public KisTransactionBasedCommand {
    LazyInitGlobalSelection(KisImageSP image, KisNodeSP activeNode)
        : m_image(image)
        , m_activeNode(activeNode)
    {}

    KisImageSP m_image;
    KisNodeSP m_activeNode;

    KUndo2Command* paint() override {
        return !KisSelectionUtils::activeSelectionForNode(m_image, m_activeNode) ?
            new KisSetEmptyGlobalSelectionCommand(m_image) : nullptr;
    }
};


void KisSelectionToolHelper::selectPixelSelection(KisPixelSelectionSP selection, SelectionAction action)
{
    KisProcessingApplicator applicator(m_image,
                                       0 /* we need no automatic updates */,
                                       KisProcessingApplicator::SUPPORTS_WRAPAROUND_MODE,
                                       KisImageSignalVector(),
                                       m_name);

    selectPixelSelection(applicator, selection, action);

    applicator.end();

}

void KisSelectionToolHelper::selectPixelSelection(KisProcessingApplicator& applicator, KisPixelSelectionSP selection, SelectionAction action)
{
    applicator.applyCommand(new LazyInitGlobalSelection(m_image, m_activeNode), KisStrokeJobData::SEQUENTIAL);

    struct ApplyToPixelSelection : public KisTransactionBasedCommand {
        ApplyToPixelSelection(KisImageSP image,
                              KisNodeSP activeNode,
                              KisPixelSelectionSP selection,
                              SelectionAction action)
            : m_image(image)
            , m_activeNode(activeNode)
            , m_selection(selection)
            , m_action(action)
        {}

        KisImageSP m_image;
        KisNodeSP m_activeNode;
        KisPixelSelectionSP m_selection;
        SelectionAction m_action;

        KUndo2Command* paint() override {

            KUndo2Command *savedCommand = 0;
            if (!m_selection->selectedExactRect().isEmpty()) {

                KisSelectionSP selection =
                    KisSelectionUtils::activeSelectionForNode(m_image, m_activeNode);
                KIS_SAFE_ASSERT_RECOVER(selection) { return 0; }

                KisPixelSelectionSP pixelSelection = selection->pixelSelection();
                KIS_SAFE_ASSERT_RECOVER(pixelSelection) { return 0; }

                bool hasSelection = !pixelSelection->isEmpty();

                KisSelectionTransaction transaction(pixelSelection);

                if (!hasSelection && m_action == SELECTION_SYMMETRICDIFFERENCE) {
                    m_action = SELECTION_REPLACE;
                }

                if (!hasSelection && m_action == SELECTION_SUBTRACT) {
                    pixelSelection->invert();
                }

                pixelSelection->applySelection(m_selection, m_action);

                QRect dirtyRect = m_image->bounds();
                if (hasSelection &&
                    m_action != SELECTION_REPLACE &&
                    m_action != SELECTION_INTERSECT &&
                    m_action != SELECTION_SYMMETRICDIFFERENCE) {

                    dirtyRect = m_selection->selectedRect();
                }
                selection->updateProjection(dirtyRect);

                savedCommand = transaction.endAndTake();
                pixelSelection->setDirty(dirtyRect);

                // release resources: transaction will care about
                // undo/redo, we don't need the selection anymore
                m_selection.clear();
            }

            KisSelectionSP activeSelection =
                KisSelectionUtils::activeSelectionForNode(m_image, m_activeNode);
            if (activeSelection && activeSelection->selectedExactRect().isEmpty()) {
                KUndo2Command *deselectCommand =
                    new KisDeselectActiveSelectionCommand(activeSelection, m_image);
                if (savedCommand) {
                    KisCommandUtils::CompositeCommand *cmd = new KisCommandUtils::CompositeCommand();
                    cmd->addCommand(savedCommand);
                    cmd->addCommand(deselectCommand);
                    savedCommand = cmd;
                } else {
                    savedCommand = deselectCommand;
                }
            }

            return savedCommand;
        }
    };

    applicator.applyCommand(
        new ApplyToPixelSelection(m_image, m_activeNode, selection, action),
        KisStrokeJobData::SEQUENTIAL);

}

void KisSelectionToolHelper::addSelectionShape(KoShape* shape, SelectionAction action)
{
    QList<KoShape*> shapes;
    shapes.append(shape);
    addSelectionShapes(shapes, action);
}
#include "krita_utils.h"
void KisSelectionToolHelper::addSelectionShapes(QList< KoShape* > shapes, SelectionAction action)
{
    if (m_image->wrapAroundModePermitted()) {
        if (KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback *>(m_canvas)) {
            feedback->showFloatingMessage(
                i18n("Shape selection does not fully "
                     "support wraparound mode. Please "
                     "use pixel selection instead"),
                KisIconUtils::loadIcon("selection-info"));
        }
    }

    KisProcessingApplicator applicator(m_image,
                                       0 /* we need no automatic updates */,
                                       KisProcessingApplicator::NONE,
                                       KisImageSignalVector(),
                                       m_name);

    applicator.applyCommand(new LazyInitGlobalSelection(m_image, m_activeNode));

    struct ClearPixelSelection : public KisTransactionBasedCommand {
        ClearPixelSelection(KisImageSP image, KisNodeSP activeNode)
            : m_image(image)
            , m_activeNode(activeNode)
        {}

        KisImageSP m_image;
        KisNodeSP m_activeNode;

        KUndo2Command* paint() override {
            KisSelectionSP selection =
                KisSelectionUtils::activeSelectionForNode(m_image, m_activeNode);
            KIS_ASSERT_RECOVER(selection) { return 0; }
            KisPixelSelectionSP pixelSelection = selection->pixelSelection();
            KIS_ASSERT_RECOVER(pixelSelection) { return 0; }

            KisSelectionTransaction transaction(pixelSelection);
            pixelSelection->clear();
            return transaction.endAndTake();
        }
    };

    if (action == SELECTION_REPLACE || action == SELECTION_DEFAULT) {
        applicator.applyCommand(new ClearPixelSelection(m_image, m_activeNode));
    }

    struct AddSelectionShape : public KisTransactionBasedCommand {
        AddSelectionShape(KoCanvasBase *canvas,
                          KisImageSP image,
                          KisNodeSP activeNode,
                          QList<KoShape*> shapes,
                          SelectionAction action)
            : m_canvas(canvas)
            , m_image(image)
            , m_activeNode(activeNode)
            , m_shapes(shapes)
            , m_action(action)
        {}

        KoCanvasBase *m_canvas;
        KisImageSP m_image;
        KisNodeSP m_activeNode;
        QList<KoShape*> m_shapes;
        SelectionAction m_action;

        KUndo2Command* paint() override {
            KUndo2Command *resultCommand = 0;

            KisSelectionSP selection =
                KisSelectionUtils::activeSelectionForNode(m_image, m_activeNode);
            if (selection) {
                KisShapeSelection * shapeSelection = static_cast<KisShapeSelection*>(selection->shapeSelection());

                if (shapeSelection ||
                        m_action == SELECTION_SUBTRACT) {

                    QPainterPath path1;
                    QList<KoShape*> existingShapes;

                    if (shapeSelection) {
                        existingShapes = shapeSelection->shapes();

                        path1.setFillRule(Qt::WindingFill);
                        Q_FOREACH(KoShape *shape, existingShapes) {
                            path1 += shape->absoluteTransformation().map(shape->outline());
                        }
                    } else if (m_action == SELECTION_SUBTRACT) {
                        const auto *converter =
                            dynamic_cast<const KisCoordinatesConverter *>(m_canvas->viewConverter());
                        KIS_SAFE_ASSERT_RECOVER(converter) { return nullptr; }
                        path1.addRect(converter->imageRectInDocumentPixels());
                    }

                    QPainterPath path2;
                    path2.setFillRule(Qt::WindingFill);
                    Q_FOREACH(KoShape *shape, m_shapes) {
                        path2 += shape->absoluteTransformation().map(shape->outline());
                    }

                    const QTransform booleanWorkaroundTransform =
                        KritaUtils::pathShapeBooleanSpaceWorkaround(m_image);

                    path1 = booleanWorkaroundTransform.map(path1);
                    path2 = booleanWorkaroundTransform.map(path2);

                    QPainterPath path = path2;

                    switch (m_action) {
                    case SELECTION_DEFAULT:
                    case SELECTION_REPLACE:
                        path = path2;
                        break;

                    case SELECTION_INTERSECT:
                        path = path1 & path2;
                        path = KritaUtils::tryCloseTornSubpathsAfterIntersection(path);
                        break;
                    case SELECTION_ADD:
                        path = path1 | path2;
                        break;

                    case SELECTION_SUBTRACT:
                        path = path1 - path2;
                        break;
                    case SELECTION_SYMMETRICDIFFERENCE:
                        path = (path1 | path2) - (path1 & path2);
                        break;
                    }

                    path = booleanWorkaroundTransform.inverted().map(path);

                    KoShape *newShape = KoPathShape::createShapeFromPainterPath(path);
                    newShape->setUserData(new KisShapeSelectionMarker);

                    KUndo2Command *parentCommand = new KUndo2Command();

                    if (!existingShapes.isEmpty()) {
                        m_canvas->shapeController()->removeShapes(existingShapes, parentCommand);
                    }
                    m_canvas->shapeController()->addShape(newShape, 0, parentCommand);

                    if (path.isEmpty()) {
                        KisCommandUtils::CompositeCommand *cmd = new KisCommandUtils::CompositeCommand();
                        cmd->addCommand(parentCommand);
                        cmd->addCommand(new KisDeselectActiveSelectionCommand(selection, m_image));
                        parentCommand = cmd;
                    }

                    resultCommand = parentCommand;
                } else if (m_action == SELECTION_INTERSECT) {
                    // just do nothing if there is nothing to intersect with
                    return nullptr;
                }
            }

            if (!resultCommand) {
                /**
                 * Mark the shapes that they belong to a shape selection
                 */
                Q_FOREACH(KoShape *shape, m_shapes) {
                    if(!shape->userData()) {
                        shape->setUserData(new KisShapeSelectionMarker);
                    }
                }

                resultCommand = m_canvas->shapeController()->addShapesDirect(m_shapes, 0);
            }
            return resultCommand;
        }
    };

    applicator.applyCommand(
        new KisGuiContextCommand(
            new AddSelectionShape(m_canvas, m_image, m_activeNode, shapes, action),
            m_canvas));
    applicator.end();
}

bool KisSelectionToolHelper::canShortcutToDeselect(const QRect &rect, SelectionAction action)
{
    return rect.isEmpty() && (action == SELECTION_INTERSECT || action == SELECTION_REPLACE);
}

bool KisSelectionToolHelper::canShortcutToNoop(const QRect &rect, SelectionAction action)
{
    return rect.isEmpty() && action == SELECTION_ADD;
}

bool KisSelectionToolHelper::tryDeselectCurrentSelection(const QRectF selectionViewRect, SelectionAction action)
{
    bool result = false;

    if (KisAlgebra2D::maxDimension(selectionViewRect) < KisConfig(true).selectionViewSizeMinimum() &&
        (action == SELECTION_INTERSECT || action == SELECTION_SYMMETRICDIFFERENCE || action == SELECTION_REPLACE)) {

        // Queueing this action to ensure we avoid a race condition when unlocking the node system
        const KisImageSP image = m_image;
        const KisNodeSP activeNode = m_activeNode;
        QTimer::singleShot(0, m_canvas, [image, activeNode]() {
            KisSelectionSP selection =
                KisSelectionUtils::activeSelectionForNode(image, activeNode);
            if (selection) {
                KisProcessingApplicator::runSingleCommandStroke(
                    image,
                    new KisDeselectActiveSelectionCommand(selection, image),
                    KisStrokeJobData::SEQUENTIAL,
                    KisStrokeJobData::EXCLUSIVE);
            }
        });
        result = true;
    }

    return result;
}

SelectionMode KisSelectionToolHelper::tryOverrideSelectionMode(KisSelectionSP activeSelection, SelectionMode currentMode, SelectionAction currentAction) const
{
    if (currentAction != SELECTION_DEFAULT && currentAction != SELECTION_REPLACE) {
        if (activeSelection) {
            currentMode = activeSelection->hasShapeSelection() ? SHAPE_PROTECTION : PIXEL_SELECTION;
        }
    }

    return currentMode;
}
