/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006-2008 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006-2010 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2008-2009 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2008 C. Boemann <cbo@boemann.dk>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "DefaultTool.h"
#include "DefaultToolFactory.h"
#include "DefaultToolDeferred.h"
#include "SelectionDecorator.h"
#include "ShapeMoveStrategy.h"
#include "ShapeRotateStrategy.h"
#include "ShapeShearStrategy.h"
#include "ShapeResizeStrategy.h"

#include <KoPointerEvent.h>
#include <KoToolSelection.h>
#include <KoToolManager.h>
#include <KoSelection.h>
#include <KoShapeController.h>
#include <KoShapeManager.h>
#include <KoSelectedShapesProxy.h>
#include <KoShapeGroup.h>
#include <KoShapeLayer.h>
#include <KoPathShape.h>
#include <KoDrag.h>
#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoCanvasResourcesIds.h>
#include <KoShapeRubberSelectStrategy.h>
#include <KoSvgTextShape.h>
#include <commands/KoShapeMoveCommand.h>
#include <commands/KoShapeTransformCommand.h>
#include <commands/KoShapeDeleteCommand.h>
#include <commands/KoShapeCreateCommand.h>
#include <commands/KoShapeGroupCommand.h>
#include <commands/KoShapeUngroupCommand.h>
#include <commands/KoShapeDistributeCommand.h>
#include <commands/KoKeepShapesSelectedCommand.h>
#include <commands/KoShapeMergeTextPropertiesCommand.h>
#include <commands/KoSvgConvertTextTypeCommand.h>
#include <commands/KoSvgTextAddRemoveShapeCommands.h>
#include <commands/KoSvgTextFlipShapeContourTypeCommand.h>
#include <commands/KoSvgTextReorderShapeInsideCommand.h>
#include <commands/KoSvgTextPathInfoChangeCommand.h>

#include <KoSnapGuide.h>
#include "kis_image.h"
#include "kis_node.h"
#include "kis_shape_controller.h"
#include "KisCanvasFeedback.h"
#include <kis_signal_compressor.h>
#include <KoInteractionStrategyFactory.h>
#include <KisHandlePainterHelper.h>


#include <PkPainterPath.h>
#include <PkPointer.h>
#include <KisSignalMapper.h>
#include <KoResourcePaths.h>

#include <KoCanvasController.h>

#include <math.h>
#include "kis_assert.h"
#include "kis_global.h"
#include "kis_debug.h"
#include "krita_utils.h"

#include <PkVectorND.h>

#define HANDLE_DISTANCE 10
#define HANDLE_DISTANCE_SQ (HANDLE_DISTANCE * HANDLE_DISTANCE)

#define INNER_HANDLE_DISTANCE_SQ 16

namespace {
static const PkString EditFillGradientFactoryId = "edit_fill_gradient";
static const PkString EditStrokeGradientFactoryId = "edit_stroke_gradient";
static const PkString EditFillMeshGradientFactoryId = "edit_fill_meshgradient";

enum TransformActionType {
    TransformRotate90CW,
    TransformRotate90CCW,
    TransformRotate180,
    TransformMirrorX,
    TransformMirrorY,
    TransformReset
};

enum BooleanOp {
    BooleanUnion,
    BooleanIntersection,
    BooleanSubtraction
};

}

class NopInteractionStrategy : public KoInteractionStrategy
{
public:
    explicit NopInteractionStrategy(KoToolBase *parent)
        : KoInteractionStrategy(parent)
    {
    }

    KUndo2Command *createCommand() override
    {
        return 0;
    }

    void handleMouseMove(const PkPointF & /*mouseLocation*/, Qt::KeyboardModifiers /*modifiers*/) override {}
    void finishInteraction(Qt::KeyboardModifiers /*modifiers*/) override {}

    void paint(PkPainter &painter, const KoViewConverter &converter) override {
        (void)painter;
        (void)converter;
    }
};

class SelectionInteractionStrategy : public KoShapeRubberSelectStrategy
{
public:
    explicit SelectionInteractionStrategy(KoToolBase *parent, const PkPointF &clicked, bool useSnapToGrid)
        : KoShapeRubberSelectStrategy(parent, clicked, useSnapToGrid)
    {
    }

    void paint(PkPainter &painter, const KoViewConverter &converter) override {
        KoShapeRubberSelectStrategy::paint(painter, converter);
    }

    void cancelInteraction() override
    {
        tool()->canvas()->updateCanvas(selectedRectangle() | tool()->decorationsRect());
    }

    void finishInteraction(Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers()) override
    {
        (void)modifiers;
        DefaultTool *defaultTool = dynamic_cast<DefaultTool*>(tool());
        KIS_SAFE_ASSERT_RECOVER_RETURN(defaultTool);

        KoSelection * selection = defaultTool->koSelection();

        const bool useContainedMode = currentMode() == CoveringSelection;

        PkList<KoShape *> shapes =
                defaultTool->shapeManager()->
                        shapesAt(selectedRectangle(), true, useContainedMode);

        for (KoShape * shape : shapes) {
                if (!shape->isSelectable()) continue;

                selection->select(shape);
            }

        tool()->canvas()->updateCanvas(selectedRectangle() | tool()->decorationsRect());
    }
};
#include <KoGradientBackground.h>
#include "KoShapeGradientHandles.h"
#include "ShapeGradientEditStrategy.h"

class DefaultTool::MoveGradientHandleInteractionFactory : public KoInteractionStrategyFactory
{
public:
    MoveGradientHandleInteractionFactory(KoFlake::FillVariant fillVariant,
                                         int priority, const PkString &id, DefaultTool *_q)
        : KoInteractionStrategyFactory(priority, id),
          q(_q),
          m_fillVariant(fillVariant)
    {
    }

    KoInteractionStrategy* createStrategy(KoPointerEvent *ev) override
    {
        m_currentHandle = handleAt(ev->point);

        if (m_currentHandle.type != KoShapeGradientHandles::Handle::None) {
            KoShape *shape = onlyEditableShape();
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, 0);

            return new ShapeGradientEditStrategy(q, m_fillVariant, shape, m_currentHandle.type, ev->point);
        }

        return 0;
    }

    bool hoverEvent(KoPointerEvent *ev) override
    {
        m_currentHandle = handleAt(ev->point);
        return false;
    }

    bool paintOnHover(PkPainter &painter, const KoViewConverter &converter) override
    {
        (void)painter;
        (void)converter;
        return false;
    }

    bool tryUseCustomCursor() override {
        if (m_currentHandle.type != KoShapeGradientHandles::Handle::None) {
            q->useCursor(Qt::OpenHandCursor);
            return true;
        }

        return false;
    }

private:

    KoShape* onlyEditableShape() const {
        KoSelection *selection = q->koSelection();
        PkList<KoShape*> shapes = selection->selectedEditableShapes();

        KoShape *shape = 0;
        if (shapes.size() == 1) {
            shape = shapes.first();
        }

        return shape;
    }

    KoShapeGradientHandles::Handle handleAt(const PkPointF &pos) {
        KoShapeGradientHandles::Handle result;

        KoShape *shape = onlyEditableShape();
        if (shape) {
            KoFlake::SelectionHandle globalHandle = q->handleAt(pos);
            const qreal distanceThresholdSq =
                globalHandle == KoFlake::NoHandle ?
                    HANDLE_DISTANCE_SQ : 0.25 * HANDLE_DISTANCE_SQ;

            const KoViewConverter *converter = q->canvas()->viewConverter();
            const PkPointF viewPoint = converter->documentToView(pos);
            qreal minDistanceSq = std::numeric_limits<qreal>::max();

            KoShapeGradientHandles sh(m_fillVariant, shape);
            for (const KoShapeGradientHandles::Handle &handle : sh.handles()) {
                const PkPointF handlePoint = converter->documentToView(handle.pos);
                const qreal distanceSq = kisSquareDistance(viewPoint, handlePoint);

                if (distanceSq < distanceThresholdSq && distanceSq < minDistanceSq) {
                    result = handle;
                    minDistanceSq = distanceSq;
                }
            }
        }

        return result;
    }

private:
    DefaultTool *q;
    KoFlake::FillVariant m_fillVariant;
    KoShapeGradientHandles::Handle m_currentHandle;
};

#include "KoShapeMeshGradientHandles.h"
#include "ShapeMeshGradientEditStrategy.h"

class DefaultTool::MoveMeshGradientHandleInteractionFactory: public KoInteractionStrategyFactory
{
public:
    MoveMeshGradientHandleInteractionFactory(KoFlake::FillVariant fillVariant,
                                             int priority,
                                             const PkString& id,
                                             DefaultTool* _q)
        : KoInteractionStrategyFactory(priority, id)
        , m_fillVariant(fillVariant)
        , q(_q)
    {
    }

    KoInteractionStrategy* createStrategy(KoPointerEvent *ev) override
    {
        m_currentHandle = handleAt(ev->point);
        q->m_selectedMeshHandle = m_currentHandle;

        if (m_currentHandle.type != KoShapeMeshGradientHandles::Handle::None) {
            KoShape *shape = onlyEditableShape();
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, 0);

            return new ShapeMeshGradientEditStrategy(q, m_fillVariant, shape, m_currentHandle, ev->point);
        }

        return nullptr;
    }

    bool hoverEvent(KoPointerEvent *ev) override
    {
        // for custom cursor
        KoShapeMeshGradientHandles::Handle handle = handleAt(ev->point);

        // refresh
        if (handle.type != m_currentHandle.type && handle.type == KoShapeMeshGradientHandles::Handle::None) {
            q->repaintDecorations();
        }

        m_currentHandle = handle;
        q->m_hoveredMeshHandle = m_currentHandle;

        // highlight the decoration which is being hovered
        if (m_currentHandle.type != KoShapeMeshGradientHandles::Handle::None) {
            q->repaintDecorations();
        }
        return false;
    }

    bool paintOnHover(PkPainter &painter, const KoViewConverter &converter) override
    {
        (void)painter;
        (void)converter;
        return false;
    }

    bool tryUseCustomCursor() override
    {
        if (m_currentHandle.type != KoShapeMeshGradientHandles::Handle::None) {
            q->useCursor(Qt::OpenHandCursor);
            return true;
        }

        return false;
    }


private:
    KoShape* onlyEditableShape() const {
        // FIXME: copy of KoShapeGradientHandles
        KoSelection *selection = q->koSelection();
        PkList<KoShape*> shapes = selection->selectedEditableShapes();

        KoShape *shape = 0;
        if (shapes.size() == 1) {
            shape = shapes.first();
        }

        return shape;
    }

    KoShapeMeshGradientHandles::Handle handleAt(const PkPointF &pos) const
    {
        // FIXME: copy of KoShapeGradientHandles. use a template?
        KoShapeMeshGradientHandles::Handle result;

        KoShape *shape = onlyEditableShape();
        if (shape) {
            KoFlake::SelectionHandle globalHandle = q->handleAt(pos);
            const qreal distanceThresholdSq =
                globalHandle == KoFlake::NoHandle ?
                    HANDLE_DISTANCE_SQ : 0.25 * HANDLE_DISTANCE_SQ;

            const KoViewConverter *converter = q->canvas()->viewConverter();
            const PkPointF viewPoint = converter->documentToView(pos);
            qreal minDistanceSq = std::numeric_limits<qreal>::max();

            KoShapeMeshGradientHandles sh(m_fillVariant, shape);

            for (const auto& handle: sh.handles()) {
                const PkPointF handlePoint = converter->documentToView(handle.pos);
                const qreal distanceSq = kisSquareDistance(viewPoint, handlePoint);

                if (distanceSq < distanceThresholdSq && distanceSq < minDistanceSq) {
                    result = handle;
                    minDistanceSq = distanceSq;
                }
            }
        }

        return result;
    }

private:
    KoFlake::FillVariant m_fillVariant;
    KoShapeMeshGradientHandles::Handle m_currentHandle;
    DefaultTool *q;
};

class SelectionHandler : public KoToolSelection
{
public:
    SelectionHandler(DefaultTool *parent)
        : KoToolSelection(parent)
        , m_selection(parent->koSelection())
    {
    }

    bool hasSelection() override
    {
        if (m_selection) {
            return m_selection->count();
        }
        return false;
    }

private:
    PkPointer<KoSelection> m_selection;
};

DefaultTool::DefaultTool(KoCanvasBase *canvas, bool connectToSelectedShapesProxy)
    : KoInteractionTool(canvas)
    , m_lastHandle(KoFlake::NoHandle)
    , m_hotPosition(KoFlake::TopLeft)
    , m_mouseWasInsideHandles(false)
    , m_textOutlineHelper(new KoSvgTextShapeOutlineHelper(canvas))
    , m_selectionHandler(new SelectionHandler(this))
    , m_textPropertyInterface(new DefaultToolTextPropertiesInterface(this))
    , m_platformServices(dynamic_cast<DefaultToolPlatformServices *>(canvas))
{
    DefaultToolFactory actionFactory;
    for (DefaultToolAction *toolAction : actionFactory.createActionsImpl()) {
        m_actions.insert(toolAction->objectName(), toolAction);
    }
    setupActions();

    for (const auto &descriptor : defaultToolCursorDescriptors()) {
        if (descriptor.kind == DefaultToolCursorKind::Rotate) {
            m_rotateCursors[descriptor.slot] = DefaultToolCursor(descriptor);
        } else if (descriptor.kind == DefaultToolCursorKind::Shear) {
            m_shearCursors[descriptor.slot] = DefaultToolCursor(descriptor);
        } else {
            m_sizeCursors[descriptor.slot] = DefaultToolCursor(descriptor);
        }
    }

    if (connectToSelectedShapesProxy) {
        PkObject::connect(canvas->selectedShapesProxy(), &KoSelectedShapesProxy::selectionChanged,
                          this, &DefaultTool::updateActions);
        PkObject::connect(canvas->selectedShapesProxy(), &KoSelectedShapesProxy::selectionChanged,
                          this, &DefaultTool::repaintDecorations);
        PkObject::connect(canvas->selectedShapesProxy(), &KoSelectedShapesProxy::selectionChanged,
                          m_textPropertyInterface, &DefaultToolTextPropertiesInterface::slotSelectionChanged);
        PkObject::connect(canvas->selectedShapesProxy(), &KoSelectedShapesProxy::selectionContentChanged,
                          this, &DefaultTool::repaintDecorations);
    }

    m_textOutlineHelper->setDrawBoundingRect(false);
    m_textOutlineHelper->setDrawShapeOutlines(true);
}

DefaultTool::~DefaultTool()
{
    for (DefaultToolAction *toolAction : m_actions.values()) {
        delete toolAction;
    }
}

DefaultToolAction *DefaultTool::action(const PkString &actionId) const
{
    return m_actions.value(actionId);
}

bool DefaultTool::dispatchAction(DefaultToolActionId actionId, bool checked)
{
    const DefaultToolActionDescriptor *descriptor = defaultToolActionDescriptor(actionId);
    if (!descriptor) return false;

    DefaultToolAction *toolAction = action(descriptor->actionId);
    if (!toolAction) return false;
    if (!toolAction->isEnabled()) return false;
    toolAction->setChecked(checked);

    if (descriptor->scope == DefaultToolActionScope::Host) {
        m_lastHostDispatchResult = false;
        toolAction->triggered();
        return m_lastHostDispatchResult;
    }

    toolAction->triggered();
    return true;
}

DefaultToolActionState DefaultTool::actionState(DefaultToolActionId actionId) const
{
    const DefaultToolActionDescriptor *descriptor = defaultToolActionDescriptor(actionId);
    if (!descriptor) return {actionId, false, false};
    const DefaultToolAction *toolAction = action(descriptor->actionId);
    if (!toolAction) return {actionId, false, false};
    return {actionId, toolAction->isEnabled(), toolAction->isChecked()};
}

DefaultToolMenuState DefaultTool::menuState() const
{
    return {action("object_unite") && action("object_unite")->isEnabled() ||
                action("object_intersect") && action("object_intersect")->isEnabled() ||
                action("object_subtract") && action("object_subtract")->isEnabled() ||
                action("object_split") && action("object_split")->isEnabled(),
            action("object_group") && action("object_group")->isEnabled() ||
                action("object_ungroup") && action("object_ungroup")->isEnabled()};
}

void DefaultTool::refreshPlatformActionState()
{
    updateActions();
}

void DefaultTool::slotActivateEditFillGradient(bool value)
{
    if (value) {
        addInteractionFactory(
            new MoveGradientHandleInteractionFactory(KoFlake::Fill,
                                                     1, EditFillGradientFactoryId, this));
    } else {
        removeInteractionFactory(EditFillGradientFactoryId);
    }
    repaintDecorations();
}

void DefaultTool::slotActivateEditStrokeGradient(bool value)
{
    if (value) {
        addInteractionFactory(
            new MoveGradientHandleInteractionFactory(KoFlake::StrokeFill,
                                                     0, EditStrokeGradientFactoryId, this));
    } else {
        removeInteractionFactory(EditStrokeGradientFactoryId);
    }
    repaintDecorations();
}

void DefaultTool::slotActivateEditFillMeshGradient(bool value)
{
    if (value) {
        addInteractionFactory(
            new MoveMeshGradientHandleInteractionFactory(KoFlake::Fill, 1,
                                                         EditFillMeshGradientFactoryId, this));
    } else {
        removeInteractionFactory(EditFillMeshGradientFactoryId);
    }
}

void DefaultTool::slotResetMeshGradientState()
{
    m_selectedMeshHandle = KoShapeMeshGradientHandles::Handle();
}

void DefaultTool::slotChangeTextType(int index)
{
    PkList<KoShape *> shapes = koSelection()->selectedShapes();

    if (shapes.isEmpty()) return;

    const KoSvgTextShape::TextType type = KoSvgTextShape::TextType(index);
    KUndo2Command *parentCommand = new KUndo2Command();
    bool convertableShape = false;
    new KoKeepShapesSelectedCommand(shapes, {}, canvas()->selectedShapesProxy(), KisCommandUtils::FlipFlopCommand::State::INITIALIZING, parentCommand);
    for (KoShape *shape : shapes) {
        KoSvgTextShape *textShape = dynamic_cast<KoSvgTextShape*>(shape);
        if (textShape && textShape->textType() != type) {
            KoSvgConvertTextTypeCommand *cmd = new KoSvgConvertTextTypeCommand(textShape, type, 0, parentCommand);
            if (!convertableShape) {
                convertableShape = true;
                parentCommand->setText(cmd->text());
            }
            KoSvgTextRemoveShapeCommand::removeContourShapesFromFlow(textShape, parentCommand, textShape->textType() == KoSvgTextShape::TextInShape, type == KoSvgTextShape::InlineWrap);
        }
    }

    new KoKeepShapesSelectedCommand({}, shapes, canvas()->selectedShapesProxy(), KisCommandUtils::FlipFlopCommand::State::FINALIZING, parentCommand);
    if (convertableShape) {
        canvas()->addCommand(parentCommand);
    }
}

void DefaultTool::slotAddShapesToFlow()
{
    KoSvgTextShape *textShape = nullptr;
    PkList<KoShape*> shapes;
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();
    std::sort(selectedShapes.begin(), selectedShapes.end(), KoShape::compareShapeZIndex);
    if (selectedShapes.isEmpty()) return;

    for (KoShape *shape : selectedShapes) {
        KoSvgTextShape *text = dynamic_cast<KoSvgTextShape*>(shape);
        KoPathShape *path = dynamic_cast<KoPathShape*>(shape);
        if (text && !textShape) {
            textShape = text;
        } else if (path && path->isClosedSubpath(0)) {
            shapes.append(shape);
        }
    }
    if (!textShape) return;
    if (shapes.isEmpty()) return;

   KUndo2Command *parentCommand = new KUndo2Command(kundo2_i18n("Add shapes to text flow."));

   if (textShape->textType() != KoSvgTextShape::InlineWrap) {
       new KoSvgConvertTextTypeCommand(textShape, KoSvgTextShape::PreformattedText, 0, parentCommand);
   }
   KoSvgTextRemoveShapeCommand::removeContourShapesFromFlow(textShape, parentCommand, false, true);

    new KoKeepShapesSelectedCommand(selectedShapes, {}, canvas()->selectedShapesProxy(), false, parentCommand);
    for (KoShape *shape : shapes) {
        new KoSvgTextAddShapeCommand(textShape, shape, true, parentCommand);
    }
    new KoKeepShapesSelectedCommand({}, {textShape}, canvas()->selectedShapesProxy(), true, parentCommand);

    canvas()->addCommand(parentCommand);
    selection->deselectAll();
    selection->select(textShape);
}

void DefaultTool::slotPutTextOnPath()
{
    KoSvgTextShape *textShape = nullptr;
    KoPathShape *textPath = nullptr;
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();
    std::sort(selectedShapes.begin(), selectedShapes.end(), KoShape::compareShapeZIndex);
    if (selectedShapes.isEmpty()) return;

    for (KoShape *shape : selectedShapes) {
        KoSvgTextShape *text = dynamic_cast<KoSvgTextShape*>(shape);
        if (text && !textShape) {
            textShape = text;
        } else if (KoPathShape *path = dynamic_cast<KoPathShape*>(shape)){
            textPath = path;
        }
        if (textShape && textPath) {
            break;
        }
    }
    if (!(textShape && textPath)) return;

   KUndo2Command *parentCommand = new KUndo2Command(kundo2_i18n("Put Text On Path"));

   new KoKeepShapesSelectedCommand(selectedShapes, {}, canvas()->selectedShapesProxy(), false, parentCommand);
   if (textShape->textType() != KoSvgTextShape::PreformattedText && textShape->textType() != KoSvgTextShape::PrePositionedText) {
       new KoSvgConvertTextTypeCommand(textShape, KoSvgTextShape::PreformattedText, 0, parentCommand);
   }

   /// This will always remove all previous text paths.
   /// While Krita's layout engine can handle multiple of them, the interaction
   /// hasn't been fully verified yet. So if someone implements multiple textpaths,
   /// they will also need to check if the cursor interaction makes sense.
   KoSvgTextRemoveShapeCommand::removeContourShapesFromFlow(textShape, parentCommand, true, true);
   new KoSvgTextSetTextPathOnRangeCommand(textShape, textPath, 0, textShape->posForIndex(textShape->plainText().size()), parentCommand);

   /// We need to adjust the startOffset by the anchor/direction, because otherwise the text might be largely off the path.
   /// This isn't a problem when using cursor to set it, as the cursor-pos can be the startOffset position.
   KoSvgText::TextAnchor anchor = KoSvgText::TextAnchor(textShape->textProperties().propertyOrDefault(KoSvgTextProperties::TextAnchorId).toInt());
   if ((textShape->direction() == KoSvgText::DirectionRightToLeft && anchor == KoSvgText::AnchorStart)
           || (textShape->direction() == KoSvgText::DirectionLeftToRight && anchor == KoSvgText::AnchorEnd)) {
       KoSvgText::TextOnPathInfo info;
       info.startOffset = 100.0;
       info.startOffsetIsPercentage = true;
       new KoSvgTextPathInfoChangeCommand(textShape, 0, info, parentCommand);
   } else if (anchor == KoSvgText::AnchorMiddle) {
       KoSvgText::TextOnPathInfo info;
       info.startOffset = 50.0;
       info.startOffsetIsPercentage = true;
       new KoSvgTextPathInfoChangeCommand(textShape, 0, info, parentCommand);
   }

   new KoKeepShapesSelectedCommand({}, {textShape}, canvas()->selectedShapesProxy(), true, parentCommand);

   canvas()->addCommand(parentCommand);
   selection->deselectAll();
   selection->select(textShape);
}

void DefaultTool::slotSubtractShapesFromFlow()
{
    KoSvgTextShape *textShape = nullptr;
    PkList<KoShape*> shapes;
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();
    std::sort(selectedShapes.begin(), selectedShapes.end(), KoShape::compareShapeZIndex);
    if (selectedShapes.isEmpty()) return;

    for (KoShape *shape : selectedShapes) {
        KoSvgTextShape *text = dynamic_cast<KoSvgTextShape*>(shape);
        KoPathShape *path = dynamic_cast<KoPathShape*>(shape);
        if (text && !textShape) {
            textShape = text;
        } else if (path && path->isClosedSubpath(0)) {
            shapes.append(shape);
        }
    }
    if (!textShape) return;
    if (shapes.isEmpty()) return;

    KUndo2Command *parentCommand = new KUndo2Command(kundo2_i18n("Subtract shapes from text flow."));

    if (textShape->textType() == KoSvgTextShape::InlineWrap) {
        new KoSvgConvertTextTypeCommand(textShape, KoSvgTextShape::PreformattedText, 0, parentCommand);
    }
    KoSvgTextRemoveShapeCommand::removeContourShapesFromFlow(textShape, parentCommand, false, true);

    new KoKeepShapesSelectedCommand(selectedShapes, {}, canvas()->selectedShapesProxy(), false, parentCommand);
    for (KoShape *shape : shapes) {
        new KoSvgTextAddShapeCommand(textShape, shape, false, parentCommand);
    }
    new KoKeepShapesSelectedCommand({}, {textShape}, canvas()->selectedShapesProxy(), true, parentCommand);

    canvas()->addCommand(parentCommand);
    selection->deselectAll();
    selection->select(textShape);
}

KoSvgTextShape* DefaultTool::tryFetchCurrentShapeManagerOwnerTextShape() const
{
    return dynamic_cast<KoSvgTextShape*>(canvas()->currentShapeManagerOwnerShape());
}

void DefaultTool::slotRemoveShapesFromFlow()
{
    KoSvgTextShape *textShape = tryFetchCurrentShapeManagerOwnerTextShape();
    if (!textShape) return;
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();
    if (selectedShapes.isEmpty()) return;

    KUndo2Command *parentCommand = new KUndo2Command(kundo2_i18n("Remove shapes from text flow."));

    new KoKeepShapesSelectedCommand(selectedShapes, {}, canvas()->selectedShapesProxy(), false, parentCommand);
    for (KoShape *shape : selectedShapes) {
        if (!textShape->shapeInContours(shape)) continue;
        new KoSvgTextRemoveShapeCommand(textShape, shape, parentCommand);
    }
    new KoKeepShapesSelectedCommand({}, selectedShapes, canvas()->selectedShapesProxy(), true, parentCommand);

    canvas()->addCommand(parentCommand);
}

void DefaultTool::slotToggleFlowShapeType()
{
    KoSvgTextShape *textShape = tryFetchCurrentShapeManagerOwnerTextShape();
    if (!textShape) return;
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();
    if (selectedShapes.isEmpty()) return;
    KUndo2Command *parentCommand = new KUndo2Command(kundo2_i18n("Toggle Flow Shape Type"));

    bool addToCanvas = false;
    for (KoShape *shape : selectedShapes) {
        if (!textShape->shapesInside().contains(shape)
                && !textShape->shapesSubtract().contains(shape)) continue;
        addToCanvas = true;
        new KoSvgTextFlipShapeContourTypeCommand(textShape, shape, parentCommand);
    }
    if (addToCanvas) {
        canvas()->addCommand(parentCommand);
    }
}

void DefaultTool::slotReorderFlowShapes(int type)
{
    KoSvgTextShape *textShape = tryFetchCurrentShapeManagerOwnerTextShape();
    if (!textShape) return;
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();
    if (selectedShapes.isEmpty()) return;

    KUndo2Command *parentCommand = new KUndo2Command();
    if (type == KoSvgTextReorderShapeInsideCommand::BringToFront) {
        parentCommand->setText(kundo2_i18n("Set Flow Shape as First"));
    } else if (type == KoSvgTextReorderShapeInsideCommand::MoveEarlier) {
        parentCommand->setText(kundo2_i18n("Decrease Flow Shape Index"));
    } else if (type == KoSvgTextReorderShapeInsideCommand::MoveLater) {
        parentCommand->setText(kundo2_i18n("Increase Flow Shape Index"));
    } else {
        parentCommand->setText(kundo2_i18n("Set Flow Shape as Last"));
    }

    PkList<KoShape *> shapesInside;
    for (KoShape *shape : selectedShapes) {
        if (!textShape->shapesInside().contains(shape)) continue;
        shapesInside.append(shape);
    }

    if (!shapesInside.isEmpty()) {
        new KoSvgTextReorderShapeInsideCommand(textShape, shapesInside, KoSvgTextReorderShapeInsideCommand::MoveShapeType(type), parentCommand);
        canvas()->addCommand(parentCommand);
    }
}

bool DefaultTool::updateTextContourMode()
{
    return m_textOutlineHelper->updateTextContourMode();
}

bool DefaultTool::wantsAutoScroll() const
{
    return true;
}

void DefaultTool::addMappedAction(KisSignalMapper *mapper, const PkString &actionId, int commandType)
{
    DefaultToolAction *a = action(actionId);
    PkObject::connect(a, &DefaultToolAction::triggered, mapper,
                      [mapper, a] { mapper->map(a); });
    mapper->setMapping(a, commandType);
}

void DefaultTool::setupActions()
{
    for (const auto &descriptor : defaultToolActionDescriptors()) {
        if (descriptor.scope != DefaultToolActionScope::Host) continue;
        DefaultToolAction *hostAction = action(descriptor.actionId);
        if (!hostAction) continue;
        PkObject::connect(hostAction, &DefaultToolAction::triggered, this,
                          [this, descriptor, hostAction] {
            m_lastHostDispatchResult = m_platformServices &&
                m_platformServices->dispatchDefaultToolHostAction(
                    descriptor.action, hostAction->isChecked());
        });
    }

    m_alignSignalsMapper = new KisSignalMapper(this);

    addMappedAction(m_alignSignalsMapper, "object_align_horizontal_left", KoShapeAlignCommand::HorizontalLeftAlignment);
    addMappedAction(m_alignSignalsMapper, "object_align_horizontal_center", KoShapeAlignCommand::HorizontalCenterAlignment);
    addMappedAction(m_alignSignalsMapper, "object_align_horizontal_right", KoShapeAlignCommand::HorizontalRightAlignment);
    addMappedAction(m_alignSignalsMapper, "object_align_vertical_top", KoShapeAlignCommand::VerticalTopAlignment);
    addMappedAction(m_alignSignalsMapper, "object_align_vertical_center", KoShapeAlignCommand::VerticalCenterAlignment);
    addMappedAction(m_alignSignalsMapper, "object_align_vertical_bottom", KoShapeAlignCommand::VerticalBottomAlignment);

    m_distributeSignalsMapper = new KisSignalMapper(this);

    addMappedAction(m_distributeSignalsMapper, "object_distribute_horizontal_left", KoShapeDistributeCommand::HorizontalLeftDistribution);
    addMappedAction(m_distributeSignalsMapper, "object_distribute_horizontal_center", KoShapeDistributeCommand::HorizontalCenterDistribution);
    addMappedAction(m_distributeSignalsMapper, "object_distribute_horizontal_right", KoShapeDistributeCommand::HorizontalRightDistribution);
    addMappedAction(m_distributeSignalsMapper, "object_distribute_horizontal_gaps", KoShapeDistributeCommand::HorizontalGapsDistribution);

    addMappedAction(m_distributeSignalsMapper, "object_distribute_vertical_top", KoShapeDistributeCommand::VerticalTopDistribution);
    addMappedAction(m_distributeSignalsMapper, "object_distribute_vertical_center", KoShapeDistributeCommand::VerticalCenterDistribution);
    addMappedAction(m_distributeSignalsMapper, "object_distribute_vertical_bottom", KoShapeDistributeCommand::VerticalBottomDistribution);
    addMappedAction(m_distributeSignalsMapper, "object_distribute_vertical_gaps", KoShapeDistributeCommand::VerticalGapsDistribution);

    m_transformSignalsMapper = new KisSignalMapper(this);

    addMappedAction(m_transformSignalsMapper, "object_transform_rotate_90_cw", TransformRotate90CW);
    addMappedAction(m_transformSignalsMapper, "object_transform_rotate_90_ccw", TransformRotate90CCW);
    addMappedAction(m_transformSignalsMapper, "object_transform_rotate_180", TransformRotate180);
    addMappedAction(m_transformSignalsMapper, "object_transform_mirror_horizontally", TransformMirrorX);
    addMappedAction(m_transformSignalsMapper, "object_transform_mirror_vertically", TransformMirrorY);
    addMappedAction(m_transformSignalsMapper, "object_transform_reset", TransformReset);

    m_booleanSignalsMapper = new KisSignalMapper(this);

    addMappedAction(m_booleanSignalsMapper, "object_unite", BooleanUnion);
    addMappedAction(m_booleanSignalsMapper, "object_intersect", BooleanIntersection);
    addMappedAction(m_booleanSignalsMapper, "object_subtract", BooleanSubtraction);

    m_textTypeSignalsMapper = new KisSignalMapper(this);
    addMappedAction(m_textTypeSignalsMapper, "text_type_preformatted", KoSvgTextShape::PreformattedText);
    addMappedAction(m_textTypeSignalsMapper, "text_type_inline_wrap", KoSvgTextShape::InlineWrap);
    addMappedAction(m_textTypeSignalsMapper, "text_type_pre_positioned", KoSvgTextShape::PrePositionedText);

    if (!action("text_type_preformatted")->actionGroup()) {
        DefaultToolActionGroup *textTypeActions = new DefaultToolActionGroup(this);
        textTypeActions->addAction(action("text_type_preformatted"));
        textTypeActions->addAction(action("text_type_inline_wrap"));
        textTypeActions->addAction(action("text_type_pre_positioned"));
        textTypeActions->setExclusive(false);
        for (DefaultToolAction *a : textTypeActions->actions()) {
            a->setCheckable(false);
        }
    }

    m_textFlowSignalsMapper  = new KisSignalMapper(this);

    addMappedAction(m_textFlowSignalsMapper, "flow_shape_order_back", KoSvgTextReorderShapeInsideCommand::SendToBack);
    addMappedAction(m_textFlowSignalsMapper, "flow_shape_order_earlier", KoSvgTextReorderShapeInsideCommand::MoveEarlier);
    addMappedAction(m_textFlowSignalsMapper, "flow_shape_order_later", KoSvgTextReorderShapeInsideCommand::MoveLater);
    addMappedAction(m_textFlowSignalsMapper, "flow_shape_order_front", KoSvgTextReorderShapeInsideCommand::BringToFront);

    m_contextMenu.reset(new DefaultToolMenu());
}

qreal DefaultTool::rotationOfHandle(KoFlake::SelectionHandle handle, bool useEdgeRotation)
{
    PkPointF selectionCenter = koSelection()->absolutePosition();
    PkPointF direction;

    switch (handle) {
    case KoFlake::TopMiddleHandle:
        if (useEdgeRotation) {
            direction = koSelection()->absolutePosition(KoFlake::TopRight)
                        - koSelection()->absolutePosition(KoFlake::TopLeft);
        } else {
            PkPointF handlePosition = koSelection()->absolutePosition(KoFlake::TopLeft);
            handlePosition += 0.5 * (koSelection()->absolutePosition(KoFlake::TopRight) - handlePosition);
            direction = handlePosition - selectionCenter;
        }
        break;
    case KoFlake::TopRightHandle:
        direction = (PkVector2D(koSelection()->absolutePosition(KoFlake::TopRight) - koSelection()->absolutePosition(KoFlake::TopLeft)).normalized() + PkVector2D(koSelection()->absolutePosition(KoFlake::TopRight) - koSelection()->absolutePosition(KoFlake::BottomRight)).normalized()).toPointF();
        break;
    case KoFlake::RightMiddleHandle:
        if (useEdgeRotation) {
            direction = koSelection()->absolutePosition(KoFlake::BottomRight)
                        - koSelection()->absolutePosition(KoFlake::TopRight);
        } else {
            PkPointF handlePosition = koSelection()->absolutePosition(KoFlake::TopRight);
            handlePosition += 0.5 * (koSelection()->absolutePosition(KoFlake::BottomRight) - handlePosition);
            direction = handlePosition - selectionCenter;
        }
        break;
    case KoFlake::BottomRightHandle:
        direction = (PkVector2D(koSelection()->absolutePosition(KoFlake::BottomRight) - koSelection()->absolutePosition(KoFlake::BottomLeft)).normalized() + PkVector2D(koSelection()->absolutePosition(KoFlake::BottomRight) - koSelection()->absolutePosition(KoFlake::TopRight)).normalized()).toPointF();
        break;
    case KoFlake::BottomMiddleHandle:
        if (useEdgeRotation) {
            direction = koSelection()->absolutePosition(KoFlake::BottomLeft)
                        - koSelection()->absolutePosition(KoFlake::BottomRight);
        } else {
            PkPointF handlePosition = koSelection()->absolutePosition(KoFlake::BottomLeft);
            handlePosition += 0.5 * (koSelection()->absolutePosition(KoFlake::BottomRight) - handlePosition);
            direction = handlePosition - selectionCenter;
        }
        break;
    case KoFlake::BottomLeftHandle:
        direction = koSelection()->absolutePosition(KoFlake::BottomLeft) - selectionCenter;
        direction = (PkVector2D(koSelection()->absolutePosition(KoFlake::BottomLeft) - koSelection()->absolutePosition(KoFlake::BottomRight)).normalized() + PkVector2D(koSelection()->absolutePosition(KoFlake::BottomLeft) - koSelection()->absolutePosition(KoFlake::TopLeft)).normalized()).toPointF();
        break;
    case KoFlake::LeftMiddleHandle:
        if (useEdgeRotation) {
            direction = koSelection()->absolutePosition(KoFlake::TopLeft)
                        - koSelection()->absolutePosition(KoFlake::BottomLeft);
        } else {
            PkPointF handlePosition = koSelection()->absolutePosition(KoFlake::TopLeft);
            handlePosition += 0.5 * (koSelection()->absolutePosition(KoFlake::BottomLeft) - handlePosition);
            direction = handlePosition - selectionCenter;
        }
        break;
    case KoFlake::TopLeftHandle:
        direction = koSelection()->absolutePosition(KoFlake::TopLeft) - selectionCenter;
        direction = (PkVector2D(koSelection()->absolutePosition(KoFlake::TopLeft) - koSelection()->absolutePosition(KoFlake::TopRight)).normalized() + PkVector2D(koSelection()->absolutePosition(KoFlake::TopLeft) - koSelection()->absolutePosition(KoFlake::BottomLeft)).normalized()).toPointF();
        break;
    case KoFlake::NoHandle:
        return 0.0;
        break;
    }

    qreal rotation = atan2(direction.y(), direction.x()) * 180.0 / M_PI;

    switch (handle) {
    case KoFlake::TopMiddleHandle:
        if (useEdgeRotation) {
            rotation -= 0.0;
        } else {
            rotation -= 270.0;
        }
        break;
    case KoFlake::TopRightHandle:
        rotation -= 315.0;
        break;
    case KoFlake::RightMiddleHandle:
        if (useEdgeRotation) {
            rotation -= 90.0;
        } else {
            rotation -= 0.0;
        }
        break;
    case KoFlake::BottomRightHandle:
        rotation -= 45.0;
        break;
    case KoFlake::BottomMiddleHandle:
        if (useEdgeRotation) {
            rotation -= 180.0;
        } else {
            rotation -= 90.0;
        }
        break;
    case KoFlake::BottomLeftHandle:
        rotation -= 135.0;
        break;
    case KoFlake::LeftMiddleHandle:
        if (useEdgeRotation) {
            rotation -= 270.0;
        } else {
            rotation -= 180.0;
        }
        break;
    case KoFlake::TopLeftHandle:
        rotation -= 225.0;
        break;
    default:
        ;
    }

    if (rotation < 0.0) {
        rotation += 360.0;
    }

    return rotation;
}

void DefaultTool::updateCursor()
{
    if (tryUseCustomCursor()) return;

    DefaultToolCursor cursor = Qt::ArrowCursor;

    PkString statusText;

    KoSelection *selection = koSelection();
    if (selection && selection->count() > 0) { // has a selection
        bool editable = !selection->selectedEditableShapes().isEmpty();

        if (!m_mouseWasInsideHandles) {
            m_angle = rotationOfHandle(m_lastHandle, true);
            int rotOctant = 8 + int(8.5 + m_angle / 45);

            bool rotateHandle = false;
            bool shearHandle = false;
            switch (m_lastHandle) {
            case KoFlake::TopMiddleHandle:
                cursor = m_shearCursors[(0 + rotOctant) % 8];
                shearHandle = true;
                break;
            case KoFlake::TopRightHandle:
                cursor = m_rotateCursors[(1 + rotOctant) % 8];
                rotateHandle = true;
                break;
            case KoFlake::RightMiddleHandle:
                cursor = m_shearCursors[(2 + rotOctant) % 8];
                shearHandle = true;
                break;
            case KoFlake::BottomRightHandle:
                cursor = m_rotateCursors[(3 + rotOctant) % 8];
                rotateHandle = true;
                break;
            case KoFlake::BottomMiddleHandle:
                cursor = m_shearCursors[(4 + rotOctant) % 8];
                shearHandle = true;
                break;
            case KoFlake::BottomLeftHandle:
                cursor = m_rotateCursors[(5 + rotOctant) % 8];
                rotateHandle = true;
                break;
            case KoFlake::LeftMiddleHandle:
                cursor = m_shearCursors[(6 + rotOctant) % 8];
                shearHandle = true;
                break;
            case KoFlake::TopLeftHandle:
                cursor = m_rotateCursors[(7 + rotOctant) % 8];
                rotateHandle = true;
                break;
            case KoFlake::NoHandle:
                cursor = Qt::ArrowCursor;
                break;
            }
            if (rotateHandle) {
                statusText = PkString("Left click rotates around center, right click around highlighted position.");
            }
            if (shearHandle) {
                statusText = PkString("Click and drag to shear selection.");
            }


        } else {
            statusText = PkString("Click and drag to resize selection.");
            m_angle = rotationOfHandle(m_lastHandle, false);
            int rotOctant = 8 + int(8.5 + m_angle / 45);
            bool cornerHandle = false;
            switch (m_lastHandle) {
            case KoFlake::TopMiddleHandle:
                cursor = m_sizeCursors[(0 + rotOctant) % 8];
                break;
            case KoFlake::TopRightHandle:
                cursor = m_sizeCursors[(1 + rotOctant) % 8];
                cornerHandle = true;
                break;
            case KoFlake::RightMiddleHandle:
                cursor = m_sizeCursors[(2 + rotOctant) % 8];
                break;
            case KoFlake::BottomRightHandle:
                cursor = m_sizeCursors[(3 + rotOctant) % 8];
                cornerHandle = true;
                break;
            case KoFlake::BottomMiddleHandle:
                cursor = m_sizeCursors[(4 + rotOctant) % 8];
                break;
            case KoFlake::BottomLeftHandle:
                cursor = m_sizeCursors[(5 + rotOctant) % 8];
                cornerHandle = true;
                break;
            case KoFlake::LeftMiddleHandle:
                cursor = m_sizeCursors[(6 + rotOctant) % 8];
                break;
            case KoFlake::TopLeftHandle:
                cursor = m_sizeCursors[(7 + rotOctant) % 8];
                cornerHandle = true;
                break;
            case KoFlake::NoHandle:
                cursor = Qt::SizeAllCursor;
                statusText = PkString("Click and drag to move selection.");
                break;
            }
            if (cornerHandle) {
                statusText = PkString("Click and drag to resize selection. Middle click to set highlighted position.");
            }
        }
        if (!editable) {
            cursor = Qt::ArrowCursor;
        }
    } else {
        // there used to be guides... :'''(
    }
    if (m_platformServices) {
        m_platformServices->useDefaultToolCursor(cursor.descriptor());
    }
    if (currentStrategy() == 0) {
        statusTextChanged(statusText);
    }
}

void DefaultTool::paint(PkPainter &painter, const KoViewConverter &converter)
{
    KoSelection *selection = koSelection();
    if (selection) {
        m_decorator.reset(new SelectionDecorator(canvas()->resourceManager()));

        {
            /**
             * Selection masks don't render the outline of the shapes, so we should
             * do that explicitly when rendering them via selection
             */

            KisNodeSP node = canvas()->resourceManager()
                                     ->resource(KoCanvasResource::CurrentKritaNode)
                                     .value<KisNodeWSP>();
            const bool isSelectionMask = node && node->inherits("KisSelectionMask");
            m_decorator->setForceShapeOutlines(isSelectionMask);


        }

        m_decorator->setSelection(selection);
        m_decorator->setHandleRadius(handleRadius());
        m_decorator->setDecorationThickness(decorationThickness());
        m_decorator->setShowFillGradientHandles(hasInteractionFactory(EditFillGradientFactoryId));
        m_decorator->setShowStrokeFillGradientHandles(hasInteractionFactory(EditStrokeGradientFactoryId));
        m_decorator->setShowFillMeshGradientHandles(hasInteractionFactory(EditFillMeshGradientFactoryId));
        m_decorator->setCurrentMeshGradientHandles(m_selectedMeshHandle, m_hoveredMeshHandle);
        m_decorator->paint(painter, converter);
    }

    m_textOutlineHelper->setHandleRadius(handleRadius());
    m_textOutlineHelper->setDecorationThickness(decorationThickness());
    m_textOutlineHelper->paint(&painter, converter);

    KoInteractionTool::paint(painter, converter);

    painter.save();
    painter.setTransform(converter.documentToView(), true);
    canvas()->snapGuide()->paint(painter, converter);
    painter.restore();
}

bool DefaultTool::isValidForCurrentLayer() const
{
    // if the currently active node has a shape manager, then it is
    // probably our client :)

    auto *shapeController = dynamic_cast<KisShapeController *>(
        canvas()->shapeController()->documentBase());
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shapeController, false);

    KisNodeSP node = canvas()->resourceManager()
                             ->resource(KoCanvasResource::CurrentKritaNode)
                             .value<KisNodeWSP>();
    return shapeController->shapeManagerForNode(node) ||
           canvas()->currentShapeManagerOwnerShape();
}

KoShapeManager *DefaultTool::shapeManager() const {
    return canvas()->shapeManager();
}

void DefaultTool::mousePressEvent(KoPointerEvent *event)
{
    // this tool only works on a vector layer right now, so give a warning if another layer type is trying to use it
    if (!isValidForCurrentLayer()) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback *>(canvas());
        KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
        feedback->showFloatingMessage(
                PkString("This tool only works on vector layers. You probably want the move tool."),
                {}, 2000, KisCanvasFeedback::Priority::Medium, Qt::AlignCenter);
        return;
    }

    if (KoSvgTextShape *shape = m_textOutlineHelper->contourModeButtonHovered(event->point)) {
        m_textOutlineHelper->toggleTextContourMode(shape);
        updateActions();
        event->accept();
        updateCursor();
        return;
    }
    KoInteractionTool::mousePressEvent(event);
    updateCursor();
}

void DefaultTool::mouseMoveEvent(KoPointerEvent *event)
{
    KoInteractionTool::mouseMoveEvent(event);
    if (currentStrategy() == 0 && koSelection() && koSelection()->count() > 0) {
        PkRectF bound = handlesSize();

        if (bound.contains(event->point)) {
            bool inside;
            KoFlake::SelectionHandle newDirection = handleAt(event->point, &inside);

            if (inside != m_mouseWasInsideHandles || m_lastHandle != newDirection) {
                m_lastHandle = newDirection;
                m_mouseWasInsideHandles = inside;
            }
        } else {
            m_lastHandle = KoFlake::NoHandle;
            m_mouseWasInsideHandles = false;

            // there used to be guides... :'''(
        }
    } else {
        // there used to be guides... :'''(
    }


    updateCursor();
}

PkRectF DefaultTool::handlesSize()
{
    KoSelection *selection = koSelection();
    if (!selection || !selection->count()) return PkRectF();

    recalcSelectionBox(selection);

    PkRectF bound = m_selectionOutline.boundingRect();

    // expansion Border
    if (!canvas() || !canvas()->viewConverter()) {
        return bound;
    }

    PkPointF border = canvas()->viewConverter()->viewToDocument(PkPointF(HANDLE_DISTANCE, HANDLE_DISTANCE));
    bound.adjust(-border.x(), -border.y(), border.x(), border.y());
    return bound;
}

void DefaultTool::mouseReleaseEvent(KoPointerEvent *event)
{
    KoInteractionTool::mouseReleaseEvent(event);
    updateCursor();
}

void DefaultTool::mouseDoubleClickEvent(KoPointerEvent *event)
{
    KoSelection *selection = koSelection();

    KoShape *shape = shapeManager()->shapeAt(event->point, KoFlake::ShapeOnTop);
    if (shape && selection && !selection->isSelected(shape)) {

        if (!(event->modifiers() & Qt::ShiftModifier)) {
            selection->deselectAll();
        }

        selection->select(shape);
    }

    explicitUserStrokeEndRequest();
}

bool DefaultTool::moveSelection(int direction, Qt::KeyboardModifiers modifiers)
{
    bool result = false;

    qreal x = 0.0, y = 0.0;
    if (direction == Qt::Key_Left) {
        x = -5;
    } else if (direction == Qt::Key_Right) {
        x = 5;
    } else if (direction == Qt::Key_Up) {
        y = -5;
    } else if (direction == Qt::Key_Down) {
        y = 5;
    }

    if (x != 0.0 || y != 0.0) { // actually move

        if ((modifiers & Qt::ShiftModifier) != 0) {
            x *= 10;
            y *= 10;
        } else if ((modifiers & Qt::AltModifier) != 0) { // more precise
            x /= 5;
            y /= 5;
        }

        PkList<KoShape *> shapes = koSelection()->selectedEditableShapes();

        if (!shapes.isEmpty()) {
            canvas()->addCommand(new KoShapeMoveCommand(shapes, PkPointF(x, y)));
            result = true;
        }
    }

    return result;
}

void DefaultTool::keyPressEvent(DefaultToolKeyEvent *event)
{
    KoInteractionTool::keyPressEvent(event);
    if (currentStrategy() == 0) {
        switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
            if (moveSelection(event->key(), event->modifiers())) {
                event->accept();
            }
            break;
        default:
            return;
        }
    }
}

PkRectF DefaultTool::decorationsRect() const
{
    PkRectF dirtyRect;

    if (koSelection() && koSelection()->count() > 0) {
        /// TODO: avoid cons_cast by implementing proper
        ///       caching strategy inrecalcSelectionBox() and
        ///       handlesSize()
        dirtyRect = const_cast<DefaultTool*>(this)->handlesSize();
    }

    if (canvas()->snapGuide()->isSnapping()) {
        dirtyRect |= canvas()->snapGuide()->boundingRect();
    }
    dirtyRect |= m_textOutlineHelper->decorationRect();

    return dirtyRect;
}

void DefaultTool::copy() const
{
    // all the selected shapes, not only editable!
    PkList<KoShape *> shapes = koSelection()->selectedShapes();

    if (!shapes.isEmpty()) {
        KoDrag drag;
        drag.setSvg(shapes);
        drag.addToClipboard();
    }
}

void DefaultTool::deleteSelection()
{
    PkList<KoShape *> shapes;
    foreach (KoShape *s, koSelection()->selectedShapes()) {
        if (s->isGeometryProtected()) {
            continue;
        }
        shapes << s;
    }
    if (!shapes.empty()) {
        canvas()->addCommand(canvas()->shapeController()->removeShapes(shapes));
    }
}

bool DefaultTool::paste()
{
    // we no longer have to do anything as tool Proxy will do it for us
    return false;
}

bool DefaultTool::selectAll()
{
    KIS_ASSERT(canvas());
    KIS_ASSERT(canvas()->selectedShapesProxy());
    for (KoShape *shape : canvas()->shapeManager()->shapes()) {
        if (!shape->isSelectable()) continue;
        canvas()->selectedShapesProxy()->selection()->select(shape);
    }
    repaintDecorations();

    return true;
}

void DefaultTool::deselect()
{
    KIS_ASSERT(canvas());
    KIS_ASSERT(canvas()->selectedShapesProxy());
    canvas()->selectedShapesProxy()->selection()->deselectAll();
    repaintDecorations();
}

KoSelection *DefaultTool::koSelection() const
{
    KIS_ASSERT(canvas());
    KIS_ASSERT(canvas()->selectedShapesProxy());
    return canvas()->selectedShapesProxy()->selection();
}

KoFlake::SelectionHandle DefaultTool::handleAt(const PkPointF &point, bool *innerHandleMeaning)
{
    // check for handles in this order; meaning that when handles overlap the one on top is chosen
    static const KoFlake::SelectionHandle handleOrder[] = {
        KoFlake::BottomRightHandle,
        KoFlake::TopLeftHandle,
        KoFlake::BottomLeftHandle,
        KoFlake::TopRightHandle,
        KoFlake::BottomMiddleHandle,
        KoFlake::RightMiddleHandle,
        KoFlake::LeftMiddleHandle,
        KoFlake::TopMiddleHandle,
        KoFlake::NoHandle
    };

    const KoViewConverter *converter = canvas()->viewConverter();
    KoSelection *selection = koSelection();

    if (!selection || !selection->count() || !converter) {
        return KoFlake::NoHandle;
    }

    recalcSelectionBox(selection);

    if (innerHandleMeaning) {
        PkPainterPath path;
        path.addPolygon(m_selectionOutline);
        *innerHandleMeaning = path.contains(point) || path.intersects(handlePaintRect(point));
    }

    const PkPointF viewPoint = converter->documentToView(point);

    for (int i = 0; i < KoFlake::NoHandle; ++i) {
        KoFlake::SelectionHandle handle = handleOrder[i];

        const PkPointF handlePoint = converter->documentToView(m_selectionBox[handle]);
        const qreal distanceSq = kisSquareDistance(viewPoint, handlePoint);

        // if just inside the outline
        if (distanceSq < HANDLE_DISTANCE_SQ) {

            if (innerHandleMeaning) {
                if (distanceSq < INNER_HANDLE_DISTANCE_SQ) {
                    *innerHandleMeaning = true;
                }
            }

            return handle;
        }
    }
    return KoFlake::NoHandle;
}

void DefaultTool::recalcSelectionBox(KoSelection *selection)
{
    KIS_ASSERT_RECOVER_RETURN(selection->count());

    PkTransform matrix = selection->absoluteTransformation();
    m_selectionOutline = matrix.map(PkPolygonF(selection->outlineRect()));
    m_angle = 0.0;

    PkPolygonF outline = m_selectionOutline; //shorter name in the following :)
    m_selectionBox[KoFlake::TopMiddleHandle] = (outline.value(0) + outline.value(1)) / 2;
    m_selectionBox[KoFlake::TopRightHandle] = outline.value(1);
    m_selectionBox[KoFlake::RightMiddleHandle] = (outline.value(1) + outline.value(2)) / 2;
    m_selectionBox[KoFlake::BottomRightHandle] = outline.value(2);
    m_selectionBox[KoFlake::BottomMiddleHandle] = (outline.value(2) + outline.value(3)) / 2;
    m_selectionBox[KoFlake::BottomLeftHandle] = outline.value(3);
    m_selectionBox[KoFlake::LeftMiddleHandle] = (outline.value(3) + outline.value(0)) / 2;
    m_selectionBox[KoFlake::TopLeftHandle] = outline.value(0);
    if (selection->count() == 1) {
#if 0        // TODO detect mirroring
        KoShape *s = koSelection()->firstSelectedShape();

        if (s->scaleX() < 0) { // vertically mirrored: swap left / right
            std::swap(m_selectionBox[KoFlake::TopLeftHandle], m_selectionBox[KoFlake::TopRightHandle]);
            std::swap(m_selectionBox[KoFlake::LeftMiddleHandle], m_selectionBox[KoFlake::RightMiddleHandle]);
            std::swap(m_selectionBox[KoFlake::BottomLeftHandle], m_selectionBox[KoFlake::BottomRightHandle]);
        }
        if (s->scaleY() < 0) { // vertically mirrored: swap top / bottom
            std::swap(m_selectionBox[KoFlake::TopLeftHandle], m_selectionBox[KoFlake::BottomLeftHandle]);
            std::swap(m_selectionBox[KoFlake::TopMiddleHandle], m_selectionBox[KoFlake::BottomMiddleHandle]);
            std::swap(m_selectionBox[KoFlake::TopRightHandle], m_selectionBox[KoFlake::BottomRightHandle]);
        }
#endif
    }
}

void DefaultTool::activate(const PkSet<KoShape *> &shapes)
{
    KoToolBase::activate(shapes);

    DefaultToolAction *actionBringToFront = action("object_order_front");
    PkObject::connect(actionBringToFront, &DefaultToolAction::triggered,
                      this, &DefaultTool::selectionBringToFront, PkConnectionType::Unique);

    DefaultToolAction *actionRaise = action("object_order_raise");
    PkObject::connect(actionRaise, &DefaultToolAction::triggered,
                      this, &DefaultTool::selectionMoveUp, PkConnectionType::Unique);

    DefaultToolAction *actionLower = action("object_order_lower");
    PkObject::connect(actionLower, &DefaultToolAction::triggered,
                      this, &DefaultTool::selectionMoveDown);

    DefaultToolAction *actionSendToBack = action("object_order_back");
    PkObject::connect(actionSendToBack, &DefaultToolAction::triggered,
                      this, &DefaultTool::selectionSendToBack, PkConnectionType::Unique);

    DefaultToolAction *actionGroupBottom = action("object_group");
    PkObject::connect(actionGroupBottom, &DefaultToolAction::triggered,
                      this, &DefaultTool::selectionGroup, PkConnectionType::Unique);

    DefaultToolAction *actionUngroupBottom = action("object_ungroup");
    PkObject::connect(actionUngroupBottom, &DefaultToolAction::triggered,
                      this, &DefaultTool::selectionUngroup, PkConnectionType::Unique);

    DefaultToolAction *actionSplit = action("object_split");
    PkObject::connect(actionSplit, &DefaultToolAction::triggered,
                      this, &DefaultTool::selectionSplitShapes, PkConnectionType::Unique);

    const auto mappedInt = static_cast<void (KisSignalMapper::*)(int)>(&KisSignalMapper::mapped);
    PkObject::connect(m_alignSignalsMapper, mappedInt, this, &DefaultTool::selectionAlign);
    PkObject::connect(m_distributeSignalsMapper, mappedInt, this, &DefaultTool::selectionDistribute);
    PkObject::connect(m_transformSignalsMapper, mappedInt, this, &DefaultTool::selectionTransform);
    PkObject::connect(m_booleanSignalsMapper, mappedInt, this, &DefaultTool::selectionBooleanOp);
    PkObject::connect(m_textTypeSignalsMapper, mappedInt, this, &DefaultTool::slotChangeTextType);
    PkObject::connect(m_textFlowSignalsMapper, mappedInt, this, &DefaultTool::slotReorderFlowShapes);

    DefaultToolAction *actionTextInside = action("add_shape_to_flow_area");
    PkObject::connect(actionTextInside, &DefaultToolAction::triggered,
                      this, &DefaultTool::slotAddShapesToFlow, PkConnectionType::Unique);

    DefaultToolAction *actionTextSubtract = action("subtract_shape_from_flow_area");
    PkObject::connect(actionTextSubtract, &DefaultToolAction::triggered,
                      this, &DefaultTool::slotSubtractShapesFromFlow, PkConnectionType::Unique);

    DefaultToolAction *actionTextOnPath = action("put_text_on_path");
    PkObject::connect(actionTextOnPath, &DefaultToolAction::triggered,
                      this, &DefaultTool::slotPutTextOnPath, PkConnectionType::Unique);

    DefaultToolAction *actionTextRemoveFlow = action("remove_shapes_from_text_flow");
    PkObject::connect(actionTextRemoveFlow, &DefaultToolAction::triggered,
                      this, &DefaultTool::slotRemoveShapesFromFlow, PkConnectionType::Unique);

    DefaultToolAction *actionTextFlowToggle = action("flow_shape_type_toggle");
    PkObject::connect(actionTextFlowToggle, &DefaultToolAction::triggered,
                      this, &DefaultTool::slotToggleFlowShapeType, PkConnectionType::Unique);

    m_mouseWasInsideHandles = false;
    m_lastHandle = KoFlake::NoHandle;
    useCursor(Qt::ArrowCursor);
    repaintDecorations();
    updateActions();

    m_textPropertyInterface->slotSelectionChanged();
}

void DefaultTool::deactivate()
{
    KoToolBase::deactivate();

    DefaultToolAction *actionBringToFront = action("object_order_front");
    actionBringToFront->disconnect();

    DefaultToolAction *actionRaise = action("object_order_raise");
    actionRaise->disconnect();

    DefaultToolAction *actionLower = action("object_order_lower");
    actionLower->disconnect();

    DefaultToolAction *actionSendToBack = action("object_order_back");
    actionSendToBack->disconnect();

    DefaultToolAction *actionGroupBottom = action("object_group");
    actionGroupBottom->disconnect();

    DefaultToolAction *actionUngroupBottom = action("object_ungroup");
    actionUngroupBottom->disconnect();

    DefaultToolAction *actionSplit = action("object_split");
    actionSplit->disconnect();

    m_alignSignalsMapper->disconnect();
    m_distributeSignalsMapper->disconnect();
    m_transformSignalsMapper->disconnect();
    m_booleanSignalsMapper->disconnect();
    m_textTypeSignalsMapper->disconnect();
    m_textFlowSignalsMapper->disconnect();

    DefaultToolAction *actionTextInside = action("add_shape_to_flow_area");
    actionTextInside->disconnect();
    DefaultToolAction *actionTextSubtract = action("subtract_shape_from_flow_area");
    actionTextSubtract->disconnect();
    DefaultToolAction *actionTextOnPath = action("put_text_on_path");
    actionTextOnPath->disconnect();
    DefaultToolAction *actionTextRemoveFlow = action("remove_shapes_from_text_flow");
    actionTextRemoveFlow->disconnect();
    DefaultToolAction *actionTextFlowToggle = action("flow_shape_type_toggle");
    actionTextFlowToggle->disconnect();

    m_textPropertyInterface->clearSelection();
}

void DefaultTool::selectionGroup()
{
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();
    std::sort(selectedShapes.begin(), selectedShapes.end(), KoShape::compareShapeZIndex);
    if (selectedShapes.isEmpty()) return;

    const int groupZIndex = selectedShapes.last()->zIndex();

    KoShapeGroup *group = new KoShapeGroup();
    group->setZIndex(groupZIndex);
    // TODO what if only one shape is left?
    KUndo2Command *cmd = new KUndo2Command(kundo2_i18n("Group shapes"));
    new KoKeepShapesSelectedCommand(selectedShapes, {}, canvas()->selectedShapesProxy(), false, cmd);
    canvas()->shapeController()->addShapeDirect(group, 0, cmd);
    new KoShapeGroupCommand(group, selectedShapes, true, cmd);
    new KoKeepShapesSelectedCommand({}, {group}, canvas()->selectedShapesProxy(), true, cmd);
    canvas()->addCommand(cmd);

    // update selection so we can ungroup immediately again
    selection->deselectAll();
    selection->select(group);
}

void DefaultTool::selectionUngroup()
{
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();
    std::sort(selectedShapes.begin(), selectedShapes.end(), KoShape::compareShapeZIndex);

    KUndo2Command *cmd = 0;
    PkList<KoShape*> newShapes;

    // add a ungroup command for each found shape container to the macro command
    for (KoShape *shape : selectedShapes) {
        KoShapeGroup *group = dynamic_cast<KoShapeGroup *>(shape);
        if (group) {
            if (!cmd) {
                cmd = new KUndo2Command(kundo2_i18n("Ungroup shapes"));
                new KoKeepShapesSelectedCommand(selectedShapes, {}, canvas()->selectedShapesProxy(), false, cmd);
            }
            newShapes << group->shapes();
            new KoShapeUngroupCommand(group, group->shapes(),
                                      group->parent() ? PkList<KoShape *>() : shapeManager()->topLevelShapes(),
                                      cmd);
            canvas()->shapeController()->removeShape(group, cmd);
        }
    }
    if (cmd) {
        new KoKeepShapesSelectedCommand({}, newShapes, canvas()->selectedShapesProxy(), true, cmd);
        canvas()->addCommand(cmd);
    }
}

void DefaultTool::selectionTransform(int transformAction)
{
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> editableShapes = selection->selectedEditableShapes();
    if (editableShapes.isEmpty()) {
        return;
    }

    PkTransform applyTransform;
    bool shouldReset = false;
    KUndo2MagicString actionName = kundo2_noi18n("BUG: No transform action");


    switch (TransformActionType(transformAction)) {
    case TransformRotate90CW:
        applyTransform.rotate(90.0);
        actionName = kundo2_i18n("Rotate Object 90° CW");
        break;
    case TransformRotate90CCW:
        applyTransform.rotate(-90.0);
        actionName = kundo2_i18n("Rotate Object 90° CCW");
        break;
    case TransformRotate180:
        applyTransform.rotate(180.0);
        actionName = kundo2_i18n("Rotate Object 180°");
        break;
    case TransformMirrorX:
        applyTransform.scale(-1.0, 1.0);
        actionName = kundo2_i18n("Mirror Object Horizontally");
        break;
    case TransformMirrorY:
        applyTransform.scale(1.0, -1.0);
        actionName = kundo2_i18n("Mirror Object Vertically");
        break;
    case TransformReset:
        shouldReset = true;
        actionName = kundo2_i18n("Reset Object Transformations");
        break;
    }

    if (!shouldReset && applyTransform.isIdentity()) return;

    PkList<PkTransform> oldTransforms;
    PkList<PkTransform> newTransforms;

    const PkRectF outlineRect = KoShape::absoluteOutlineRect(editableShapes);
    const PkPointF centerPoint = outlineRect.center();
    const PkTransform centerTrans = PkTransform::fromTranslate(centerPoint.x(), centerPoint.y());
    const PkTransform centerTransInv = PkTransform::fromTranslate(-centerPoint.x(), -centerPoint.y());

    // we also add selection to the list of transformed shapes, so that its outline is updated correctly
    PkList<KoShape*> transformedShapes = editableShapes;
    transformedShapes << selection;

    for (KoShape *shape : transformedShapes) {
        oldTransforms.append(shape->transformation());

        PkTransform t;

        if (!shouldReset) {
            const PkTransform world = shape->absoluteTransformation();
            t =  world * centerTransInv * applyTransform * centerTrans * world.inverted() * shape->transformation();
        } else {
            const PkPointF center = shape->outlineRect().center();
            const PkPointF offset = shape->transformation().map(center) - center;
            t = PkTransform::fromTranslate(offset.x(), offset.y());
        }

        newTransforms.append(t);
    }

    KoShapeTransformCommand *cmd = new KoShapeTransformCommand(transformedShapes, oldTransforms, newTransforms);
    cmd->setText(actionName);
    canvas()->addCommand(cmd);
}

void DefaultTool::selectionBooleanOp(int booleanOp)
{
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> editableShapes = selection->selectedEditableShapes();
    if (editableShapes.isEmpty()) {
        return;
    }

    PkVector<PkPainterPath> srcOutlines;
    PkPainterPath dstOutline;
    KUndo2MagicString actionName = kundo2_noi18n("BUG: boolean action name");

    // TODO: implement a reference shape selection dialog!
    const int referenceShapeIndex = 0;
    KoShape *referenceShape = editableShapes[referenceShapeIndex];

    auto *shapeController = dynamic_cast<KisShapeController *>(
        canvas()->shapeController()->documentBase());
    KIS_SAFE_ASSERT_RECOVER_RETURN(shapeController);
    KisImageSP image = shapeController->currentImage();
    KIS_SAFE_ASSERT_RECOVER_RETURN(image);
    const PkTransform booleanWorkaroundTransform =
        KritaUtils::pathShapeBooleanSpaceWorkaround(image);

    for (KoShape *shape : editableShapes) {
        srcOutlines <<
            booleanWorkaroundTransform.map(
            shape->absoluteTransformation().map(
                shape->outline()));
    }

    if (booleanOp == BooleanUnion) {
        for (const PkPainterPath &path : srcOutlines) {
            dstOutline |= path;
        }
        actionName = kundo2_i18n("Unite Shapes");
    } else if (booleanOp == BooleanIntersection) {
        for (int i = 0; i < srcOutlines.size(); i++) {
            if (i == 0) {
                dstOutline = srcOutlines[i];
            } else {
                dstOutline &= srcOutlines[i];
            }
        }

        // there is a bug in Qt, sometimes it leaves the resulting
        // outline open, so just close it explicitly.
        dstOutline.closeSubpath();

        actionName = kundo2_i18n("Intersect Shapes");

    } else if (booleanOp == BooleanSubtraction) {
        for (int i = 0; i < srcOutlines.size(); i++) {
            dstOutline = srcOutlines[referenceShapeIndex];
            if (i != referenceShapeIndex) {
                dstOutline -= srcOutlines[i];
            }
        }

        actionName = kundo2_i18n("Subtract Shapes");
    }

    dstOutline = booleanWorkaroundTransform.inverted().map(dstOutline);

    KoShape *newShape = 0;

    if (!dstOutline.isEmpty()) {
        newShape = KoPathShape::createShapeFromPainterPath(dstOutline);
    }

    KUndo2Command *cmd = new KUndo2Command(actionName);

    new KoKeepShapesSelectedCommand(editableShapes, {}, canvas()->selectedShapesProxy(), false, cmd);

    PkList<KoShape*> newSelectedShapes;

    if (newShape) {
        newShape->setBackground(referenceShape->background());
        newShape->setStroke(referenceShape->stroke());
        newShape->setZIndex(referenceShape->zIndex());

        KoShapeContainer *parent = referenceShape->parent();
        canvas()->shapeController()->addShapeDirect(newShape, parent, cmd);

        newSelectedShapes << newShape;
    }

    canvas()->shapeController()->removeShapes(editableShapes, cmd);

    new KoKeepShapesSelectedCommand({}, newSelectedShapes, canvas()->selectedShapesProxy(), true, cmd);

    canvas()->addCommand(cmd);
}

void DefaultTool::selectionSplitShapes()
{
    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> editableShapes = selection->selectedEditableShapes();
    if (editableShapes.isEmpty()) {
        return;
    }

    KUndo2Command *cmd = new KUndo2Command(kundo2_i18n("Split Shapes"));

    new KoKeepShapesSelectedCommand(editableShapes, {}, canvas()->selectedShapesProxy(), false, cmd);
    PkList<KoShape*> newShapes;

    for (KoShape *shape : editableShapes) {
        KoPathShape *pathShape = dynamic_cast<KoPathShape*>(shape);
        if (!pathShape) return;

        PkList<KoPathShape*> splitShapes;
        if (pathShape->separate(splitShapes)) {
            PkList<KoShape*> normalShapes = implicitCastList<KoShape*>(splitShapes);

            KoShapeContainer *parent = shape->parent();
            canvas()->shapeController()->addShapesDirect(normalShapes, parent, cmd);
            canvas()->shapeController()->removeShape(shape, cmd);
            newShapes << normalShapes;
        }
    }

    new KoKeepShapesSelectedCommand({}, newShapes, canvas()->selectedShapesProxy(), true, cmd);

    canvas()->addCommand(cmd);
}

void DefaultTool::selectionAlign(int _align)
{
    KoShapeAlignCommand::Align align =
        static_cast<KoShapeAlignCommand::Align>(_align);

    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> editableShapes = selection->selectedEditableShapes();
    if (editableShapes.isEmpty()) {
        return;
    }

    // TODO add an option to the widget so that one can align to the page
    // with multiple selected shapes too

    PkRectF bb;

    // single selected shape is automatically aligned to document rect
    if (editableShapes.count() == 1) {
        if (!canvas()->resourceManager()->hasResource(KoCanvasResource::PageSize)) {
            return;
        }
        bb = PkRectF(PkPointF(0, 0), canvas()->resourceManager()->sizeResource(KoCanvasResource::PageSize));
    } else {
        bb = KoShape::absoluteOutlineRect(editableShapes);
    }

    KoShapeAlignCommand *cmd = new KoShapeAlignCommand(editableShapes, align, bb);
    canvas()->addCommand(cmd);
}

void DefaultTool::selectionDistribute(int _distribute)
{
    KoShapeDistributeCommand::Distribute distribute =
        static_cast<KoShapeDistributeCommand::Distribute>(_distribute);

    KoSelection *selection = koSelection();
    if (!selection) return;

    PkList<KoShape *> editableShapes = selection->selectedEditableShapes();
    if (editableShapes.size() < 3) {
        return;
    }

    PkRectF bb = KoShape::absoluteOutlineRect(editableShapes);
    KoShapeDistributeCommand *cmd = new KoShapeDistributeCommand(editableShapes, distribute, bb);
    canvas()->addCommand(cmd);
}

void DefaultTool::selectionBringToFront()
{
    selectionReorder(KoShapeReorderCommand::BringToFront);
}

void DefaultTool::selectionMoveUp()
{
    selectionReorder(KoShapeReorderCommand::RaiseShape);
}

void DefaultTool::selectionMoveDown()
{
    selectionReorder(KoShapeReorderCommand::LowerShape);
}

void DefaultTool::selectionSendToBack()
{
    selectionReorder(KoShapeReorderCommand::SendToBack);
}

void DefaultTool::selectionReorder(KoShapeReorderCommand::MoveShapeType order)
{
    KoSelection *selection = koSelection();
    if (!selection) {
        return;
    }

    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();
    if (selectedShapes.isEmpty()) {
        return;
    }

    KUndo2Command *cmd = KoShapeReorderCommand::createCommand(selectedShapes, shapeManager(), order);
    if (cmd) {
        canvas()->addCommand(cmd);
    }
}

void DefaultTool::canvasResourceChanged(int key, const PkVariant &res)
{
    if (key == HotPosition) {
        m_hotPosition = KoFlake::AnchorPosition(res.toInt());
        repaintDecorations();
    }
}

KoInteractionStrategy *DefaultTool::createStrategy(KoPointerEvent *event)
{
    KoSelection *selection = koSelection();
    if (!selection) return nullptr;

    bool insideSelection = false;
    KoFlake::SelectionHandle handle = handleAt(event->point, &insideSelection);

    bool editableShape = !selection->selectedEditableShapes().isEmpty();

    const bool selectMultiple = event->modifiers() & Qt::ShiftModifier;
    const bool selectNextInStack = event->modifiers() & Qt::ControlModifier;
    const bool avoidSelection = event->modifiers() & Qt::AltModifier;

    if (selectNextInStack) {
        // change the hot selection position when middle clicking on a handle
        KoFlake::AnchorPosition newHotPosition = m_hotPosition;
        switch (handle) {
        case KoFlake::TopMiddleHandle:
            newHotPosition = KoFlake::Top;
            break;
        case KoFlake::TopRightHandle:
            newHotPosition = KoFlake::TopRight;
            break;
        case KoFlake::RightMiddleHandle:
            newHotPosition = KoFlake::Right;
            break;
        case KoFlake::BottomRightHandle:
            newHotPosition = KoFlake::BottomRight;
            break;
        case KoFlake::BottomMiddleHandle:
            newHotPosition = KoFlake::Bottom;
            break;
        case KoFlake::BottomLeftHandle:
            newHotPosition = KoFlake::BottomLeft;
            break;
        case KoFlake::LeftMiddleHandle:
            newHotPosition = KoFlake::Left;
            break;
        case KoFlake::TopLeftHandle:
            newHotPosition = KoFlake::TopLeft;
            break;
        case KoFlake::NoHandle:
        default:
            // check if we had hit the center point
            const KoViewConverter *converter = canvas()->viewConverter();
            PkPointF pt = converter->documentToView(event->point);

            // TODO: use calculated values instead!
            PkPointF centerPt = converter->documentToView(selection->absolutePosition());

            if (kisSquareDistance(pt, centerPt) < HANDLE_DISTANCE_SQ) {
                newHotPosition = KoFlake::Center;
            }

            break;
        }

        if (m_hotPosition != newHotPosition) {
            canvas()->resourceManager()->setResource(HotPosition, newHotPosition);
            return new NopInteractionStrategy(this);
        }
    }

    if (!avoidSelection && editableShape) {
        // manipulation of selected shapes goes first
        if (handle != KoFlake::NoHandle) {
            // resizing or shearing only with left mouse button
            if (insideSelection) {
                // uniform-scaling 面板复选框已删除，但要保住原行为：DefaultToolGeometryWidget
                // 原先在 (shapes.size() > 1 || onlyGroupShape) 时强制勾上该复选框。多选的情况
                // 已经由 ShapeResizeStrategy 内部 (m_selectedShapes.size() > 1) 单独覆盖，这里
                // 只需要补上「单选且选中的是一个 group shape」这一种也应为 true 的情形。
                const PkList<KoShape*> selectedShapesForScaling = selection->selectedEditableShapes();
                bool forceUniformScaling = selectedShapesForScaling.size() == 1 &&
                    dynamic_cast<KoShapeGroup*>(selectedShapesForScaling.first());
                return new ShapeResizeStrategy(this, selection, event->point, handle, forceUniformScaling);
            }

            if (handle == KoFlake::TopMiddleHandle || handle == KoFlake::RightMiddleHandle ||
                handle == KoFlake::BottomMiddleHandle || handle == KoFlake::LeftMiddleHandle) {

                return new ShapeShearStrategy(this, selection, event->point, handle);
            }

            // rotating is allowed for right mouse button too
            if (handle == KoFlake::TopLeftHandle || handle == KoFlake::TopRightHandle ||
                    handle == KoFlake::BottomLeftHandle || handle == KoFlake::BottomRightHandle) {

                return new ShapeRotateStrategy(this, selection, event->point, event->buttons());
            }
        }

        if (!selectMultiple && !selectNextInStack) {

           if (insideSelection) {
                return new ShapeMoveStrategy(this, selection, event->point);
            }
        }
    }

    KoShape *shape = shapeManager()->shapeAt(event->point, selectNextInStack ? KoFlake::NextUnselected : KoFlake::ShapeOnTop);

    if (avoidSelection || (!shape && handle == KoFlake::NoHandle)) {
        if (!selectMultiple) {
            selection->deselectAll();
        }
        return new SelectionInteractionStrategy(this, event->point, false);
    }

    if (selection->isSelected(shape)) {
        if (selectMultiple) {
            selection->deselect(shape);
        }
    } else if (handle == KoFlake::NoHandle) { // clicked on shape which is not selected
        if (!selectMultiple) {
            selection->deselectAll();
        }
        selection->select(shape);
        // tablet selection isn't precise and may lead to a move, preventing that
        if (event->isTabletEvent()) {
            return new NopInteractionStrategy(this);
        }
        return new ShapeMoveStrategy(this, selection, event->point);
    }
    return 0;
}

void DefaultTool::updateActions()
{
    PkList<KoShape*> editableShapes;

    if (koSelection()) {
        editableShapes = koSelection()->selectedEditableShapes();
    }

    const bool hasEditableShapes = !editableShapes.isEmpty();

    action("object_order_front")->setEnabled(hasEditableShapes);
    action("object_order_raise")->setEnabled(hasEditableShapes);
    action("object_order_lower")->setEnabled(hasEditableShapes);
    action("object_order_back")->setEnabled(hasEditableShapes);

    action("object_transform_rotate_90_cw")->setEnabled(hasEditableShapes);
    action("object_transform_rotate_90_ccw")->setEnabled(hasEditableShapes);
    action("object_transform_rotate_180")->setEnabled(hasEditableShapes);
    action("object_transform_mirror_horizontally")->setEnabled(hasEditableShapes);
    action("object_transform_mirror_vertically")->setEnabled(hasEditableShapes);
    action("object_transform_reset")->setEnabled(hasEditableShapes);

    const bool multipleSelected = editableShapes.size() > 1;

    const bool alignmentEnabled =
       multipleSelected ||
       (!editableShapes.isEmpty() &&
        canvas()->resourceManager()->hasResource(KoCanvasResource::PageSize));

    action("object_align_horizontal_left")->setEnabled(alignmentEnabled);
    action("object_align_horizontal_center")->setEnabled(alignmentEnabled);
    action("object_align_horizontal_right")->setEnabled(alignmentEnabled);
    action("object_align_vertical_top")->setEnabled(alignmentEnabled);
    action("object_align_vertical_center")->setEnabled(alignmentEnabled);
    action("object_align_vertical_bottom")->setEnabled(alignmentEnabled);

    const bool distributionEnabled = editableShapes.size() > 2;

    action("object_distribute_horizontal_left")->setEnabled(distributionEnabled);
    action("object_distribute_horizontal_center")->setEnabled(distributionEnabled);
    action("object_distribute_horizontal_right")->setEnabled(distributionEnabled);
    action("object_distribute_horizontal_gaps")->setEnabled(distributionEnabled);

    action("object_distribute_vertical_top")->setEnabled(distributionEnabled);
    action("object_distribute_vertical_center")->setEnabled(distributionEnabled);
    action("object_distribute_vertical_bottom")->setEnabled(distributionEnabled);
    action("object_distribute_vertical_gaps")->setEnabled(distributionEnabled);

    /* Handling the text actions */
    bool textShape = false;
    bool otherShapes = false;
    bool filledShapes = false;
    bool shapesInside = false;
    KoSvgTextShape *currentTextShapeGroup = tryFetchCurrentShapeManagerOwnerTextShape();
    const bool editFlowShapes = bool(currentTextShapeGroup);
    for (KoShape *shape : editableShapes) {
        KoSvgTextShape *text = dynamic_cast<KoSvgTextShape *>(shape);
        if (text && !textShape) {
            textShape = true;
        } else {
            otherShapes = true;
            KoPathShape *path = dynamic_cast<KoPathShape*>(shape);
            filledShapes = filledShapes? filledShapes: (path && path->isClosedSubpath(0));
            if (editFlowShapes) {
                if (!shapesInside && currentTextShapeGroup->shapesInside().contains(shape)) {
                    shapesInside = true;
                }
            }
        }
        if (textShape && otherShapes) break;
    }
    const bool editContours = textShape && otherShapes;
    const bool editFilledContours = textShape && filledShapes;

    action("add_shape_to_flow_area")->setEnabled(editFilledContours);
    action("subtract_shape_from_flow_area")->setEnabled(editFilledContours);
    action("put_text_on_path")->setEnabled(editContours);
    action("remove_shapes_from_text_flow")->setEnabled(editFlowShapes);
    action("flow_shape_type_toggle")->setEnabled(editFlowShapes);
    action("flow_shape_order_back")->setEnabled(shapesInside);
    action("flow_shape_order_earlier")->setEnabled(shapesInside);
    action("flow_shape_order_later")->setEnabled(shapesInside);
    action("flow_shape_order_front")->setEnabled(shapesInside);

    updateDistinctiveActions(editableShapes);

    selectionChanged(editableShapes.size());
}

void DefaultTool::updateDistinctiveActions(const PkList<KoShape*> &editableShapes) {
    const bool multipleSelected = editableShapes.size() > 1;

    action("object_group")->setEnabled(multipleSelected);

    action("object_unite")->setEnabled(multipleSelected);
    action("object_intersect")->setEnabled(multipleSelected);
    action("object_subtract")->setEnabled(multipleSelected);

    bool hasShapesWithMultipleSegments = false;
    for (KoShape *shape : editableShapes) {
            KoPathShape *pathShape = dynamic_cast<KoPathShape *>(shape);
            if (pathShape && pathShape->subpathCount() > 1) {
                hasShapesWithMultipleSegments = true;
                break;
            }
        }
    action("object_split")->setEnabled(hasShapesWithMultipleSegments);


    bool hasGroupShape = false;
            foreach (KoShape *shape, editableShapes) {
            if (dynamic_cast<KoShapeGroup *>(shape)) {
                hasGroupShape = true;
                break;
            }
        }
    action("object_ungroup")->setEnabled(hasGroupShape);

    bool enablePreformatted = false;
    bool enablePrePositioned = false;
    bool enableInlineWrapped = false;
    bool text = false;
    for (KoShape *shape : editableShapes) {
        KoSvgTextShape *textShape = dynamic_cast<KoSvgTextShape *>(shape);
        if (textShape) {
            text = true;
            if (textShape->textType() != KoSvgTextShape::PreformattedText && !enablePreformatted) {
                enablePreformatted = true;
            }
            if (textShape && textShape->textType() != KoSvgTextShape::PrePositionedText && !enablePrePositioned) {
                enablePrePositioned = true;
            }
            if (textShape && textShape->textType() != KoSvgTextShape::InlineWrap && !enableInlineWrapped) {
                enableInlineWrapped = true;
            }
        }
    }
    DefaultToolActionGroup *group = action("text_type_preformatted")->actionGroup();
    if (group) {
        group->setEnabled(text);
    }

    action("text_type_preformatted")->setEnabled(enablePreformatted);
    action("text_type_pre_positioned")->setEnabled(enablePrePositioned);
    action("text_type_inline_wrap")->setEnabled(enableInlineWrapped);
}


KoToolSelection *DefaultTool::selection()
{
    return m_selectionHandler;
}

DefaultToolMenu* DefaultTool::popupActionsMenu()
{
    if (m_contextMenu) {
        m_contextMenu->clear();

        m_contextMenu->addSection(PkString("Vector Shape Actions"));
        m_contextMenu->addSeparator();

        DefaultToolMenu *transform = m_contextMenu->addMenu(PkString("Transform"));

        transform->addAction(action("object_transform_rotate_90_cw"));
        transform->addAction(action("object_transform_rotate_90_ccw"));
        transform->addAction(action("object_transform_rotate_180"));
        transform->addSeparator();
        transform->addAction(action("object_transform_mirror_horizontally"));
        transform->addAction(action("object_transform_mirror_vertically"));
        transform->addSeparator();
        transform->addAction(action("object_transform_reset"));

        if (action("object_unite")->isEnabled() ||
            action("object_intersect")->isEnabled() ||
            action("object_subtract")->isEnabled() ||
            action("object_split")->isEnabled()) {

            DefaultToolMenu *transform = m_contextMenu->addMenu(PkString("Logical Operations"));
            transform->addAction(action("object_unite"));
            transform->addAction(action("object_intersect"));
            transform->addAction(action("object_subtract"));
            transform->addAction(action("object_split"));
        }

        m_contextMenu->addSeparator();

        m_contextMenu->addAction(action("edit_cut"));
        m_contextMenu->addAction(action("edit_copy"));
        m_contextMenu->addAction(action("edit_paste"));
        m_contextMenu->addAction(action("paste_at"));

        m_contextMenu->addSeparator();

        m_contextMenu->addAction(action("object_order_front"));
        m_contextMenu->addAction(action("object_order_raise"));
        m_contextMenu->addAction(action("object_order_lower"));
        m_contextMenu->addAction(action("object_order_back"));

        if (action("object_group")->isEnabled() || action("object_ungroup")->isEnabled()) {
            m_contextMenu->addSeparator();
            m_contextMenu->addAction(action("object_group"));
            m_contextMenu->addAction(action("object_ungroup"));
        }
        m_contextMenu->addSeparator();
        m_contextMenu->addAction(action("convert_shapes_to_vector_selection"));

        m_contextMenu->addSeparator();
        DefaultToolMenu *text = m_contextMenu->addMenu(PkString("Text"));
        text->addAction(action("add_shape_to_flow_area"));
        text->addAction(action("subtract_shape_from_flow_area"));
        text->addAction(action("put_text_on_path"));
        text->addSeparator();
        text->addAction(action("text_type_preformatted"));
        text->addAction(action("text_type_inline_wrap"));
        text->addAction(action("text_type_pre_positioned"));
        text->addSeparator();
        text->addAction(action("remove_shapes_from_text_flow"));
        text->addAction(action("flow_shape_type_toggle"));
        text->addSeparator();
        text->addAction(action("flow_shape_order_back"));
        text->addAction(action("flow_shape_order_earlier"));
        text->addAction(action("flow_shape_order_later"));
        text->addAction(action("flow_shape_order_front"));
    }

    return m_contextMenu.data();
}

void DefaultTool::addTransformActions(DefaultToolMenu *menu) const {
    menu->addAction(action("object_transform_rotate_90_cw"));
    menu->addAction(action("object_transform_rotate_90_ccw"));
    menu->addAction(action("object_transform_rotate_180"));
    menu->addSeparator();
    menu->addAction(action("object_transform_mirror_horizontally"));
    menu->addAction(action("object_transform_mirror_vertically"));
    menu->addSeparator();
    menu->addAction(action("object_transform_reset"));
}

void DefaultTool::explicitUserStrokeEndRequest()
{
    PkList<KoShape *> shapes = koSelection()->selectedEditableShapesAndDelegates();
    PkString tool = KoToolManager::instance()->preferredToolForSelection(shapes);
    DefaultToolDeferred::post(*this, [tool = std::move(tool)]() {
        KoToolManager::instance()->switchToolRequested(tool);
    });
}

struct DefaultToolTextPropertiesInterface::Private {

    Private(DefaultTool *parent)
        : parent(parent)
        , compressor(10, KisSignalCompressor::POSTPONE){}

    DefaultTool *parent;
    PkList<KoShape*> shapes;
    KisSignalCompressor compressor;
    PkConnection compressorConnection;
};

DefaultToolTextPropertiesInterface::DefaultToolTextPropertiesInterface(DefaultTool *parent)
    : KoSvgTextPropertiesInterface(parent)
    , d(new Private(parent))
{
    d->compressorConnection =
        PkObject::connect(&d->compressor, &KisSignalCompressor::timeout,
                          &d->compressor, [this]() { textSelectionChanged(); });
}

DefaultToolTextPropertiesInterface::~DefaultToolTextPropertiesInterface()
{
    PkObject::disconnect(d->compressorConnection);
    clearSelection();
}

PkList<KoSvgTextProperties> DefaultToolTextPropertiesInterface::getSelectedProperties()
{
    PkList<KoSvgTextProperties> props = PkList<KoSvgTextProperties>();
    if (!d->parent->selection()->hasSelection()) return props;

    PkList<KoShape*> shapes = d->shapes;
    for (auto it = shapes.begin(); it != shapes.end(); it++) {
        KoSvgTextShape *textShape = dynamic_cast<KoSvgTextShape*>(*it);
        if (!textShape) continue;
        KoSvgTextProperties p = textShape->textProperties();
        props.append(p);
    }

    return props;
}

PkList<KoSvgTextProperties> DefaultToolTextPropertiesInterface::getCharacterProperties()
{
    return PkList<KoSvgTextProperties>();
}

KoSvgTextProperties DefaultToolTextPropertiesInterface::getInheritedProperties()
{
    return KoSvgTextProperties();
}

void DefaultToolTextPropertiesInterface::setPropertiesOnSelected(KoSvgTextProperties properties, PkSet<KoSvgTextProperties::PropertyId> removeProperties)
{
    if (d->shapes.isEmpty()) return;
    KUndo2Command *cmd = new KoShapeMergeTextPropertiesCommand(d->shapes, properties, removeProperties);
    if (cmd) {
        d->parent->canvas()->addCommand(cmd);
    }
}

void DefaultToolTextPropertiesInterface::setCharacterPropertiesOnSelected(KoSvgTextProperties properties, PkSet<KoSvgTextProperties::PropertyId> removeProperties)
{
    (void)properties;
    (void)removeProperties;
    return;
}

bool DefaultToolTextPropertiesInterface::spanSelection()
{
    return false;
}

bool DefaultToolTextPropertiesInterface::characterPropertiesEnabled()
{
    return false;
}

void DefaultToolTextPropertiesInterface::notifyCursorPosChanged(int pos, int anchor)
{
    (void)pos;
    (void)anchor;
    d->compressor.start();
}

void DefaultToolTextPropertiesInterface::notifyMarkupChanged()
{
    d->compressor.start();
}

void DefaultToolTextPropertiesInterface::notifyShapeChanged(KoShape::ChangeType type, KoShape *shape)
{
    if (type == KoShape::Deleted)
        d->shapes.removeAll(shape);

    d->compressor.start();
}

void DefaultToolTextPropertiesInterface::clearSelection()
{
    for (KoShape *shape : d->shapes) {
        shape->removeShapeChangeListener(this);
    }
    d->shapes.clear();
}

void DefaultToolTextPropertiesInterface::slotSelectionChanged()
{
    if (d->parent->updateTextContourMode()) return;
    for (KoShape *shape : d->shapes) {
        if (!shape) continue;
        shape->removeShapeChangeListener(this);
    }

    auto *textShapeGroup = d->parent->tryFetchCurrentShapeManagerOwnerTextShape();

    if (textShapeGroup) {
        d->shapes = {textShapeGroup};
    } else {
        d->shapes = d->parent->canvas()->selectedShapesProxy()->selection()->selectedEditableShapes();
    }

    for (KoShape *shape : d->shapes) {
        if (!shape) continue;
        shape->addShapeChangeListener(this);
    }
    d->compressor.start();
}
