/*
 *  kis_tool_transform.h - part of Krita
 *
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
 *  SPDX-FileCopyrightText: 2010 Marc Pegon <pe.marc@free.fr>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_TRANSFORM_H_
#define KIS_TOOL_TRANSFORM_H_

#include <PkPoint.h>
#include <PkVectorND.h>
#include <PkString.h>

#include <KoToolFactoryBase.h>

#include <kis_shape_selection.h>
#include <kis_undo_adapter.h>
#include <kis_types.h>
#include <flake/kis_node_shape.h>
#include <kis_tool.h>

#include <KisToolPaintFactoryBase.h>


#include "tool_transform_args.h"
#include "KisToolChangesTracker.h"
#include "transform_transaction_properties.h"
#include "strokes/inplace_transform_stroke_strategy.h"

class KisTransformStrategyBase;
class KisCoordinatesConverter;
class KisWarpTransformStrategy;
class KisCageTransformStrategy;
class KisLiquifyTransformStrategy;
class KisFreeTransformStrategy;
class KisPerspectiveTransformStrategy;
class KisMeshTransformStrategy;

struct TransformToolFactoryDescriptor {
    PkString id;
    PkString toolTip;
    PkString section;
    PkString iconName;
    PkString shortcut;
    PkString activationShapeId;
    int priority {0};
};

KRITATOOLTRANSFORM_EXPORT TransformToolFactoryDescriptor transformToolFactoryDescriptor();


/**
 * Transform tool
 * This tool offers several modes.
 * - Free Transform mode allows the user to translate, scale, shear, rotate and
 *   apply a perspective transformation to a selection or the whole canvas.
 * - Warp mode allows the user to warp the selection of the canvas by grabbing
 *   and moving control points placed on the image. The user can either work
 *   with default control points, like a grid whose density can be modified, or
 *   place the control points manually. The modifications made on the selected
 *   pixels are applied only when the user clicks the Apply button : the
 *   semi-transparent image displayed until the user click that button is only a
 *   preview.
 * - Cage transform is similar to warp transform with control points exactly
 *   placed on the outer boundary. The user draws a boundary polygon, the
 *   vertices of which become control points.
 * - Perspective transform applies a two-point perspective transformation. The
 *   user can manipulate the corners of the selection. If the vanishing points
 *   of the resulting quadrilateral are on screen, the user can manipulate those
 *   as well.
 * - Liquify transform transforms the selection by painting motions, as if the
 *   user was finger painting.
 */
class KisToolTransform : public KisTool
{










public:
    enum TransformToolMode {
        FreeTransformMode,
        WarpTransformMode,
        CageTransformMode,
        LiquifyTransformMode,
        PerspectiveTransformMode,
        MeshTransformMode
    };

    enum WarpType {
        RigidWarpType,
        AffineWarpType,
        SimilitudeWarpType
    };

    KisToolTransform(KoCanvasBase * canvas);
    ~KisToolTransform() override;

    /**
     * @brief wantsAutoScroll
     * reimplemented from KoToolBase
     * there's an issue where autoscrolling with this tool never makes the
     * stroke end, so we return false here so that users don't get stuck with
     * the tool. See bug 362659
     * @return false
     */
    bool wantsAutoScroll() const override {
        return false;
    }

    void mousePressEvent(KoPointerEvent *e) override;
    void mouseMoveEvent(KoPointerEvent *e) override;
    void mouseReleaseEvent(KoPointerEvent *e) override;
    void beginActionImpl(KoPointerEvent *event, bool usePrimaryAction, KisTool::AlternateAction action);
    void continueActionImpl(KoPointerEvent *event, bool usePrimaryAction, KisTool::AlternateAction action);
    void endActionImpl(KoPointerEvent *event, bool usePrimaryAction, KisTool::AlternateAction action);
    void activatePrimaryAction() override;
    void deactivatePrimaryAction() override;
    void beginPrimaryAction(KoPointerEvent *event) override;
    void continuePrimaryAction(KoPointerEvent *event) override;
    void endPrimaryAction(KoPointerEvent *event) override;

    void activateAlternateAction(AlternateAction action) override;
    void deactivateAlternateAction(AlternateAction action) override;
    void beginAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void continueAlternateAction(KoPointerEvent *event, AlternateAction action) override;
    void endAlternateAction(KoPointerEvent *event, AlternateAction action) override;

    void paint(PkPainter& gc, const KoViewConverter &converter) override;

    void newActivationWithExternalSource(KisPaintDeviceSP externalSource) override;

    void setNextActivationTransformMode(TransformToolMode mode);
    TransformToolMode transformMode() const;

    double translateX() const;
    double translateY() const;

    double rotateX() const;
    double rotateY() const;
    double rotateZ() const;

    double scaleX() const;
    double scaleY() const;

    double shearX() const;
    double shearY() const;

    WarpType warpType() const;
    double warpFlexibility() const;
    int warpPointDensity() const;

    static ToolTransformArgs::TransformMode toArgsMode(KisToolTransform::TransformToolMode toolMode);

    enum class PlatformAction {
        Free, Perspective, Warp, Cage, Liquify, Mesh,
        MirrorHorizontal, MirrorVertical, RotateClockwise, RotateCounterClockwise,
        KeepAspectRatio, Apply, Reset,
        MoveUp, MoveUpMore, MoveDown, MoveDownMore,
        MoveLeft, MoveLeftMore, MoveRight, MoveRightMore,
        IncreaseBrushSize, DecreaseBrushSize
    };
    bool dispatchPlatformAction(PlatformAction action, bool checked = false);

public:
    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;
    // Applies the current transformation to the original paint device and commits it to the undo stack
    void applyTransform();

    void requestImageRecalculation();

    void setTransformMode( KisToolTransform::TransformToolMode newMode );

    void setTranslateX(double translateX);
    void setTranslateY(double translateY);

    void setRotateX(double rotation);
    void setRotateY(double rotation);
    void setRotateZ(double rotation);

    void setScaleX(double scaleX);
    void setScaleY(double scaleY);

    void setShearX(double shearX);
    void setShearY(double shearY);

    void setWarpType(WarpType type);
    void setWarpFlexibility(double flexibility);
    void setWarpPointDensity(int density);

protected:
    void resetCursorStyle() override;
    void slotGlobalConfigChanged();

signals:
    void transformModeChanged();
    void freeTransformChanged();
    void warpTransformChanged();

public:
    void requestUndoDuringStroke() override;
    void requestRedoDuringStroke() override;
    void requestStrokeEnd() override;
    void requestStrokeCancellation() override;
    void canvasUpdateRequested();
    void cursorOutlineUpdateRequested(const PkPointF &imagePos);

    // Update the widget according to m_currentArgs
    void updateOptionWidget();

    void resetRotationCenterButtonsRequested();
    void imageTooBigRequested(bool value);
    void convexHullCalculationRequested();
    void slotConvexHullCalculated(PkPolygon hull, void *strokeStrategyCookie);

private:
    void startStroke(ToolTransformArgs::TransformMode mode, bool forceReset);
    void endStroke();
    void cancelStroke();

private:
    void outlineChanged();
    // Sets the cursor according to mouse position (doesn't take shearing into account well yet)
    void setFunctionalCursor();
    // Sets m_function according to mouse position and modifier
    void setTransformFunction(PkPointF mousePos, Qt::KeyboardModifiers modifiers);

    void commitChanges();

    void initTransformMode(ToolTransformArgs::TransformMode mode);
    void initGuiAfterTransformMode();

    void initThumbnailImage(KisPaintDeviceSP previewDevice);
    void updateApplyResetAvailability();

private:
    ToolTransformArgs m_currentArgs;

    // Set by newActivationWithExternalSource before starting a new stroke.
    // The source pixels for the next transform will be read from this device.
    KisPaintDeviceSP m_externalSourceForNextActivation;

    bool m_actuallyMoveWhileSelected {false}; // true <=> selection has been moved while clicked

    KisPaintDeviceSP m_selectedPortionCache;
    KisStrokeId m_strokeId;
    void *m_strokeStrategyCookie {0};
    bool m_currentlyUsingOverlayPreviewStyle {false};
    bool m_preferOverlayPreviewStyle {false};
    bool m_forceLodMode {false};


    PkPainterPath m_selectionPath; // original (unscaled) selection outline, used for painting decorations

    const KisCoordinatesConverter *m_converter {nullptr};

    // Cached scaleX/scaleY ratio used to keep the aspect ratio locked while
    // one axis is being changed programmatically (setScaleX/setScaleY).
    // Ported from the deleted options panel's KisToolTransformConfigWidget::m_scaleRatio.
    qreal m_scaleRatio {1.0};

    TransformTransactionProperties m_transaction;
    KisToolChangesTracker m_changesTracker;
    TransformToolMode nextActivationTransformMode {FreeTransformMode};

    /**
     * This artificial rect is used to store the image to flake
     * transformation. We check against this rect to get to know
     * whether zoom has changed.
     */
    PkRectF m_refRect;

    PkScopedPointer<KisWarpTransformStrategy> m_warpStrategy;
    PkScopedPointer<KisCageTransformStrategy> m_cageStrategy;
    PkScopedPointer<KisLiquifyTransformStrategy> m_liquifyStrategy;
    PkScopedPointer<KisMeshTransformStrategy> m_meshStrategy;
    PkScopedPointer<KisFreeTransformStrategy> m_freeStrategy;
    PkScopedPointer<KisPerspectiveTransformStrategy> m_perspectiveStrategy;
    KisTransformStrategyBase* currentStrategy() const;

    PkPainterPath m_cursorOutline;

    KisAsynchronousStrokeUpdateHelper m_asyncUpdateHelper;

private:
    void slotTrackerChangedConfig(KisToolChangesTrackerDataSP status);
    void slotUiChangedConfig(bool needsPreviewRecalculation);
    void slotApplyTransform();
    void slotResetTransform(ToolTransformArgs::TransformMode mode);
    void slotCancelTransform();
    void slotRestartTransform();
    void slotRestartAndContinueTransform();
    void slotEditingFinished();

    void slotMoveDiscreteUp();
    void slotMoveDiscreteUpMore();
    void slotMoveDiscreteDown();
    void slotMoveDiscreteDownMore();
    void slotMoveDiscreteLeft();
    void slotMoveDiscreteLeftMore();
    void slotMoveDiscreteRight();
    void slotMoveDiscreteRightMore();

    void slotIncreaseBrushSize();
    void slotDecreaseBrushSize();

    void slotTransactionGenerated(TransformTransactionProperties transaction, ToolTransformArgs args, void *strokeStrategyCookie);
    void slotPreviewDeviceGenerated(KisPaintDeviceSP device);

    // context menu options for updating the transform type
    // this is to help with discoverability since come people can't find the tool options
    void slotUpdateToWarpType();
    void slotUpdateToPerspectiveType();
    void slotUpdateToFreeTransformType();
    void slotUpdateToLiquifyType();
    void slotUpdateToMeshType();
    void slotUpdateToCageType();

    // "a few extra context click options if free transform is active"
    // (mirrorHorizontalAction/mirrorVerticalAction/rotateNinetyCWAction/
    // rotateNinetyCCWAction/keepAspectRatioAction). Ported from the deleted
    // options panel's KisToolTransformConfigWidget::slotFlipX/slotFlipY/
    // slotRotateCW/slotRotateCCW/slotSetKeepAspectRatio so the context menu
    // keeps working without the panel.
    void slotFlipHorizontal();
    void slotFlipVertical();
    void slotRotateNinetyCW();
    void slotRotateNinetyCCW();
    void slotSetKeepAspectRatio(bool value);
};

class KisToolTransformFactory : public KisToolPaintFactoryBase
{


public:

    KisToolTransformFactory()
            : KisToolPaintFactoryBase(transformToolFactoryDescriptor().id) {
        const TransformToolFactoryDescriptor descriptor = transformToolFactoryDescriptor();
        setToolTip(descriptor.toolTip);
        setSection(descriptor.section);
        setIconName(descriptor.iconName);
        setShortcut(descriptor.shortcut);
        setPriority(descriptor.priority);
        setActivationShapeId(descriptor.activationShapeId);
    }

    ~KisToolTransformFactory() override {}

    KoToolBase * createTool(KoCanvasBase *canvas) override {
        return new KisToolTransform(canvas);
    }

public:
    void activateSubtool(KisToolTransform::TransformToolMode mode);
};



#endif // KIS_TOOL_TRANSFORM_H_
