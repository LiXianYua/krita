/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006-2008 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006-2008 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DEFAULTTOOL_H
#define DEFAULTTOOL_H

#include <KoInteractionTool.h>
#include <KoFlake.h>
#include <commands/KoShapeAlignCommand.h>
#include <commands/KoShapeReorderCommand.h>
#include "SelectionDecorator.h"
#include <KoSvgTextShapeOutlineHelper.h>
#include "KoShapeMeshGradientHandles.h"
#include <KoSvgTextPropertiesInterface.h>
#include "DefaultToolUi.h"
#include "DefaultToolPlatform.h"

#include <PkPolygon.h>
#include <PkDateTime.h>
#include <PkMap.h>

class KisSignalMapper;
class KoInteractionStrategy;
class KoShapeMoveCommand;
class KoSelection;
class KisViewManager;
class DefaultToolTextPropertiesInterface;

/**
 * The default tool (associated with the arrow icon) implements the default
 * interactions you have with flake objects.<br>
 * The tool provides scaling, moving, selecting, rotation and soon skewing of
 * any number of shapes.
 * <p>Note that the implementation of those different strategies are delegated
 * to the InteractionStrategy class and its subclasses.
 */
class DefaultTool : public KoInteractionTool
{

public:
    /**
     * Constructor for basic interaction tool where user actions are translated
     * and handled by interaction strategies of type KoInteractionStrategy.
     * @param canvas the canvas this tool will be working for.
     */
    explicit DefaultTool(KoCanvasBase *canvas, bool connectToSelectedShapesProxy = false);
    ~DefaultTool() override;

    enum CanvasResource {
        HotPosition = 1410100299
    };

public:

    bool wantsAutoScroll() const override;
    void paint(PkPainter &painter, const KoViewConverter &converter) override;

    PkRectF decorationsRect() const override;

    ///reimplemented
    void copy() const override;

    ///reimplemented
    void deleteSelection() override;

    ///reimplemented
    bool paste() override;

    ///reimplemented
    bool selectAll() override;

    ///reimplemented
    void deselect() override;

    ///reimplemented
    KoToolSelection *selection() override;

    DefaultToolMenu *popupActionsMenu() override;
    bool dispatchAction(DefaultToolActionId action, bool checked = false);

    /**
     * Returns which selection handle is at params point (or NoHandle if none).
     * @return which selection handle is at params point (or NoHandle if none).
     * @param point the location (in pt) where we should look for a handle
     * @param innerHandleMeaning this boolean is altered to true if the point
     *   is inside the selection rectangle and false if it is just outside.
     *   The value of innerHandleMeaning is undefined if the handle location is NoHandle
     */
    KoFlake::SelectionHandle handleAt(const PkPointF &point, bool *innerHandleMeaning = 0);

    bool updateTextContourMode();

    KoSvgTextShape* tryFetchCurrentShapeManagerOwnerTextShape() const;

public :
    void activate(const PkSet<KoShape *> &shapes) override;
    void deactivate() override;

private :
    void selectionAlign(int _align);
    void selectionDistribute(int _distribute);

    void selectionBringToFront();
    void selectionSendToBack();
    void selectionMoveUp();
    void selectionMoveDown();

    void selectionGroup();
    void selectionUngroup();

    void selectionTransform(int transformAction);
    void selectionBooleanOp(int booleanOp);
    void selectionSplitShapes();

    void slotActivateEditFillGradient(bool value);
    void slotActivateEditStrokeGradient(bool value);

    void slotActivateEditFillMeshGradient(bool value);
    void slotResetMeshGradientState();

    void slotChangeTextType(int index);
    void slotAddShapesToFlow();
    void slotPutTextOnPath();
    void slotSubtractShapesFromFlow();
    void slotRemoveShapesFromFlow();
    void slotToggleFlowShapeType();
    void slotReorderFlowShapes(int type);

protected :
    /// Update actions on selection change
    void updateActions();
    DefaultToolAction *action(const PkString &actionId) const;

public: // Events

    void mousePressEvent(KoPointerEvent *event) override;
    void mouseMoveEvent(KoPointerEvent *event) override;
    void mouseReleaseEvent(KoPointerEvent *event) override;
    void mouseDoubleClickEvent(KoPointerEvent *event) override;

    void keyPressEvent(DefaultToolKeyEvent *event) override;

    void explicitUserStrokeEndRequest() override;
protected:
    KoInteractionStrategy *createStrategy(KoPointerEvent *event) override;

protected:
    friend class SelectionInteractionStrategy;
    virtual bool isValidForCurrentLayer() const;
    virtual KoShapeManager *shapeManager() const;
    virtual KoSelection *koSelection() const;

    /**
     * Enable/disable actions specific to the tool (vector vs. reference images)
     */
    virtual void updateDistinctiveActions(const PkList<KoShape*> &editableShapes);

    void addTransformActions(DefaultToolMenu *menu) const;
    PkScopedPointer<DefaultToolMenu> m_contextMenu;

private:
    class MoveGradientHandleInteractionFactory;
    class MoveMeshGradientHandleInteractionFactory;

private:
    void setupActions();
    void recalcSelectionBox(KoSelection *selection);
    void updateCursor();
    /// Returns rotation angle of given handle of the current selection
    qreal rotationOfHandle(KoFlake::SelectionHandle handle, bool useEdgeRotation);

    void addMappedAction(KisSignalMapper *mapper, const PkString &actionId, int type);

    void selectionReorder(KoShapeReorderCommand::MoveShapeType order);
    bool moveSelection(int direction, Qt::KeyboardModifiers modifiers);

    /// Returns selection rectangle adjusted by handle proximity threshold
    PkRectF handlesSize();


    void canvasResourceChanged(int key, const PkVariant &res) override;

    KoFlake::SelectionHandle m_lastHandle;
    KoFlake::AnchorPosition m_hotPosition;
    bool m_mouseWasInsideHandles;
    PkPointF m_selectionBox[8];
    PkPolygonF m_selectionOutline;
    PkPointF m_lastPoint;

    PkScopedPointer<SelectionDecorator> m_decorator;
    PkScopedPointer<KoSvgTextShapeOutlineHelper> m_textOutlineHelper;

    KoShapeMeshGradientHandles::Handle m_selectedMeshHandle;
    KoShapeMeshGradientHandles::Handle m_hoveredMeshHandle;

    // TODO alter these 3 arrays to be static const instead
    DefaultToolCursor m_sizeCursors[8];
    DefaultToolCursor m_rotateCursors[8];
    DefaultToolCursor m_shearCursors[8];
    qreal m_angle;
    KoToolSelection *m_selectionHandler;
    friend class SelectionHandler;

    KisSignalMapper *m_alignSignalsMapper {0};
    KisSignalMapper *m_distributeSignalsMapper {0};
    KisSignalMapper *m_transformSignalsMapper {0};
    KisSignalMapper *m_booleanSignalsMapper {0};
    KisSignalMapper *m_textTypeSignalsMapper {0};
    KisSignalMapper *m_textFlowSignalsMapper {0};

    DefaultToolTextPropertiesInterface *m_textPropertyInterface{0};
    PkMap<PkString, DefaultToolAction *> m_actions;
    DefaultToolPlatformServices *m_platformServices {nullptr};
    bool m_lastHostDispatchResult {false};
};

#include <KoSvgTextShape.h>

/// Interface to interact with the text property manager.
class DefaultToolTextPropertiesInterface: public KoSvgTextPropertiesInterface, public KoSvgTextShape::TextCursorChangeListener
{

public:
    DefaultToolTextPropertiesInterface(DefaultTool *parent);
    ~DefaultToolTextPropertiesInterface();
    virtual PkList<KoSvgTextProperties> getSelectedProperties() override;
    virtual PkList<KoSvgTextProperties> getCharacterProperties() override;
    virtual KoSvgTextProperties getInheritedProperties() override;
    virtual void setPropertiesOnSelected(KoSvgTextProperties properties, PkSet<KoSvgTextProperties::PropertyId> removeProperties = PkSet<KoSvgTextProperties::PropertyId>()) override;
    virtual void setCharacterPropertiesOnSelected(KoSvgTextProperties properties, PkSet<KoSvgTextProperties::PropertyId> removeProperties = PkSet<KoSvgTextProperties::PropertyId>()) override;
    virtual bool spanSelection() override;
    virtual bool characterPropertiesEnabled() override;

    virtual void notifyCursorPosChanged(int pos, int anchor) override;
    virtual void notifyMarkupChanged() override;
    virtual void notifyShapeChanged(KoShape::ChangeType type, KoShape *shape) override;

    void clearSelection();
public :
    void textSelectionChanged()
    {
        activateSignal<>(this, PkMemberFnKey::from(&DefaultToolTextPropertiesInterface::textSelectionChanged));
    }
    void slotSelectionChanged();
private:
    struct Private;
    const PkScopedPointer<Private> d;
};


#endif
