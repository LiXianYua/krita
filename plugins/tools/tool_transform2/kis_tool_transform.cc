/*
 *  kis_tool_transform.cc -- part of Krita
 *
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
 *  SPDX-FileCopyrightText: 2010 Marc Pegon <pe.marc@free.fr>
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_transform.h"


#include <math.h>
#include <limits>

#include <PkPainter.h>
#include <PkPen.h>
#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkMatrix4x4.h>

#include <kis_debug.h>
#include "TransformToolPlatform.h"

#include <KoPointerEvent.h>
#include <KoID.h>
#include <KoCanvasBase.h>
#include <KoToolManager.h>
#include <KoViewConverter.h>
#include <KoSelection.h>
#include <KoCompositeOp.h>
#include <PkSharedConfig.h>

#include <kis_global.h>
#include <KisCanvasInvalidation.h>
#include <KisCanvasToolServices.h>
#include <kis_coordinates_converter.h>
#include <kis_painter.h>
#include <kis_image.h>
#include <kis_undo_adapter.h>
#include <kis_transaction.h>
#include <kis_selection.h>
#include <kis_filter_strategy.h>
#include <kis_transform_worker.h>
#include <kis_perspectivetransform_worker.h>
#include <kis_warptransform_worker.h>
#include <kis_pixel_selection.h>
#include <kis_shape_selection.h>
#include <krita_utils.h>
#include <kis_resources_snapshot.h>
#include <KisOptimizedBrushOutline.h>

#include <KoShapeTransformCommand.h>
#include <KoCanvasController.h>

#include "kis_transform_utils.h"
#include "kis_warp_transform_strategy.h"
#include "kis_cage_transform_strategy.h"
#include "kis_liquify_transform_strategy.h"
#include "kis_free_transform_strategy.h"
#include "kis_perspective_transform_strategy.h"
#include "kis_mesh_transform_strategy.h"

#include "kis_transform_mask.h"
#include "kis_transform_mask_adapter.h"

#include "krita_container_utils.h"
#include "kis_layer_utils.h"
#include <KisDelayedUpdateNodeInterface.h>
#include "kis_config_notifier.h"

#include "strokes/transform_stroke_strategy.h"
#include "strokes/inplace_transform_stroke_strategy.h"

KisToolTransform::KisToolTransform(KoCanvasBase * canvas)
    : KisTool(canvas, TransformCursorDescriptor{TransformCursorKind::PointingHand})
    , m_converter(dynamic_cast<const KisCoordinatesConverter *>(canvas->viewConverter()))
    , m_warpStrategy(
        new KisWarpTransformStrategy(
            m_converter,
            canvas->snapGuide(),
            m_currentArgs, m_transaction))
    , m_cageStrategy(
        new KisCageTransformStrategy(
            m_converter,
            canvas->snapGuide(),
            m_currentArgs, m_transaction))
    , m_liquifyStrategy(
        new KisLiquifyTransformStrategy(
            m_converter,
            m_currentArgs, m_transaction, canvas->resourceManager(),
            dynamic_cast<KisCanvasToolServices *>(canvas)))
    , m_meshStrategy(
        new KisMeshTransformStrategy(
            m_converter,
            canvas->snapGuide(),
            m_currentArgs, m_transaction))
    , m_freeStrategy(
        new KisFreeTransformStrategy(
            m_converter,
            canvas->snapGuide(),
            m_currentArgs, m_transaction))
    , m_perspectiveStrategy(
        new KisPerspectiveTransformStrategy(
            m_converter,
            canvas->snapGuide(),
            m_currentArgs, m_transaction))
{
    KIS_ASSERT(m_converter);

    setObjectName("tool_transform");

#define CONNECT_TRANSFORM(sender, signal, slot) \
    PkObject::connect(sender, signal, this, slot)
    CONNECT_TRANSFORM(m_warpStrategy.data(), &KisWarpTransformStrategy::requestCanvasUpdate,
                             &KisToolTransform::canvasUpdateRequested);
    CONNECT_TRANSFORM(m_warpStrategy.data(), &KisWarpTransformStrategy::requestImageRecalculation,
                             &KisToolTransform::requestImageRecalculation);
    CONNECT_TRANSFORM(m_cageStrategy.data(), &KisWarpTransformStrategy::requestCanvasUpdate,
                             &KisToolTransform::canvasUpdateRequested);
    CONNECT_TRANSFORM(m_cageStrategy.data(), &KisWarpTransformStrategy::requestImageRecalculation,
                             &KisToolTransform::requestImageRecalculation);
    CONNECT_TRANSFORM(m_liquifyStrategy.data(), &KisLiquifyTransformStrategy::requestCanvasUpdate,
                             &KisToolTransform::canvasUpdateRequested);
    CONNECT_TRANSFORM(m_liquifyStrategy.data(), &KisLiquifyTransformStrategy::requestCursorOutlineUpdate,
                             &KisToolTransform::cursorOutlineUpdateRequested);
    CONNECT_TRANSFORM(m_liquifyStrategy.data(), &KisLiquifyTransformStrategy::requestUpdateOptionWidget,
                             &KisToolTransform::updateOptionWidget);
    CONNECT_TRANSFORM(m_liquifyStrategy.data(), &KisLiquifyTransformStrategy::requestImageRecalculation,
                             &KisToolTransform::requestImageRecalculation);
    CONNECT_TRANSFORM(m_freeStrategy.data(), &KisFreeTransformStrategy::requestCanvasUpdate,
                             &KisToolTransform::canvasUpdateRequested);
    CONNECT_TRANSFORM(m_freeStrategy.data(), &KisFreeTransformStrategy::requestResetRotationCenterButtons,
                             &KisToolTransform::resetRotationCenterButtonsRequested);
    CONNECT_TRANSFORM(m_freeStrategy.data(), &KisFreeTransformStrategy::requestShowImageTooBig,
                             &KisToolTransform::imageTooBigRequested);
    CONNECT_TRANSFORM(m_freeStrategy.data(), &KisFreeTransformStrategy::requestImageRecalculation,
                             &KisToolTransform::requestImageRecalculation);
    CONNECT_TRANSFORM(m_freeStrategy.data(), &KisFreeTransformStrategy::requestConvexHullCalculation,
                             &KisToolTransform::convexHullCalculationRequested);
    CONNECT_TRANSFORM(m_perspectiveStrategy.data(), &KisPerspectiveTransformStrategy::requestCanvasUpdate,
                             &KisToolTransform::canvasUpdateRequested);
    CONNECT_TRANSFORM(m_perspectiveStrategy.data(), &KisPerspectiveTransformStrategy::requestShowImageTooBig,
                             &KisToolTransform::imageTooBigRequested);
    CONNECT_TRANSFORM(m_perspectiveStrategy.data(), &KisPerspectiveTransformStrategy::requestImageRecalculation,
                             &KisToolTransform::requestImageRecalculation);
    CONNECT_TRANSFORM(m_meshStrategy.data(), &KisMeshTransformStrategy::requestCanvasUpdate,
                             &KisToolTransform::canvasUpdateRequested);
    CONNECT_TRANSFORM(m_meshStrategy.data(), &KisMeshTransformStrategy::requestImageRecalculation,
                             &KisToolTransform::requestImageRecalculation);
    CONNECT_TRANSFORM(&m_changesTracker, &KisToolChangesTracker::sigConfigChanged,
                             &KisToolTransform::slotTrackerChangedConfig);
    CONNECT_TRANSFORM(KisConfigNotifier::instance(), &KisConfigNotifier::configChanged,
                             &KisToolTransform::slotGlobalConfigChanged);
#undef CONNECT_TRANSFORM
}

KisToolTransform::~KisToolTransform()
{
    cancelStroke();
}

void KisToolTransform::outlineChanged()
{
    freeTransformChanged();
    KisCanvasInvalidation *invalidation = dynamic_cast<KisCanvasInvalidation *>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(invalidation);
    invalidation->invalidateAll();
}

void KisToolTransform::canvasUpdateRequested()
{
    KisCanvasInvalidation *invalidation = dynamic_cast<KisCanvasInvalidation *>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(invalidation);
    invalidation->invalidateAll();
}

void KisToolTransform::resetCursorStyle()
{
    setFunctionalCursor();
}

void KisToolTransform::slotGlobalConfigChanged()
{
    PkConfigGroup group = PkSharedConfig::openConfig()->group(toolId());
    m_preferOverlayPreviewStyle = group.readEntry("useOverlayPreviewStyle", false);
    m_forceLodMode = group.readEntry("forceLodMode", true);
}

void KisToolTransform::resetRotationCenterButtonsRequested()
{
    resetTransformToolRotationCenterControls(canvas());
}

void KisToolTransform::imageTooBigRequested(bool value)
{
    setTransformToolImageTooBig(canvas(), value);
}

void KisToolTransform::convexHullCalculationRequested()
{
    if (m_strokeId && !m_transaction.rootNodes().isEmpty()) {
        /**
         * Free transform strategy issues the recalculation request every time
         * the user performs a bounds rotation action, so we should skip actual
         * recalculation, when it is not necessary anymore
         */

        if (m_transaction.convexHullHasBeenRequested()) {
            return;
        }

        m_transaction.setConvexHullHasBeenRequested(true);

        if (m_currentlyUsingOverlayPreviewStyle) {
            image()->addJob(m_strokeId, new TransformStrokeStrategy::CalculateConvexHullData());
        } else {
            image()->addJob(m_strokeId, new InplaceTransformStrokeStrategy::CalculateConvexHullData());
        }
    }
}
void KisToolTransform::slotConvexHullCalculated(PkPolygon hull, void *strokeStrategyCookie)
{
    if (!m_strokeId || strokeStrategyCookie != m_strokeStrategyCookie) return;
    PkPolygonF hullF;
    hullF.reserve(hull.size());
    for (const PkPoint &point : hull) {
        hullF.append(PkPointF(point.x(), point.y()));
    }
    /**
     * Only use the convex hull if it matches the original bounding rect.
     * When we skip setConvexHull() call, nothing serious happens, except that
     * rotatted bounds are rotated around the entire clip rect, not actual
     * clip's data.
     */
    if (hullF.boundingRect() == m_transaction.originalRect()) {
        m_transaction.setConvexHull(hullF);
        currentStrategy()->externalConfigChanged();
        canvasUpdateRequested();
    } else {
        warnTools << "WARNING: KisToolTransform: calculated convex hull's bounds "
                     "differ from the bounding rect of the source clip. It shouldn't "
                     "have happened";
    }
}

KisTransformStrategyBase* KisToolTransform::currentStrategy() const
{
    if (m_currentArgs.mode() == ToolTransformArgs::FREE_TRANSFORM) {
        return m_freeStrategy.data();
    } else if (m_currentArgs.mode() == ToolTransformArgs::WARP) {
        return m_warpStrategy.data();
    } else if (m_currentArgs.mode() == ToolTransformArgs::CAGE) {
        return m_cageStrategy.data();
    } else if (m_currentArgs.mode() == ToolTransformArgs::LIQUIFY) {
        return m_liquifyStrategy.data();
    } else if (m_currentArgs.mode() == ToolTransformArgs::MESH) {
        return m_meshStrategy.data();
    } else /* if (m_currentArgs.mode() == ToolTransformArgs::PERSPECTIVE_4POINT) */ {
        return m_perspectiveStrategy.data();
    }
}

void KisToolTransform::paint(PkPainter& gc, const KoViewConverter &converter)
{
    (void)converter;

    if (!m_strokeId || m_transaction.rootNodes().isEmpty()) return;

    PkRectF newRefRect = KisTransformUtils::imageToFlake(m_converter, PkRectF(0.0,0.0,1.0,1.0));
    if (m_refRect != newRefRect) {
        m_refRect = newRefRect;
        currentStrategy()->externalConfigChanged();
    }
    currentStrategy()->setDecorationThickness(decorationThickness());
    TransformToolPainter transformPainter(gc);
    currentStrategy()->paint(transformPainter);


    if (!m_cursorOutline.isEmpty()) {
        PkPainterPath mappedOutline =
            KisTransformUtils::imageToFlakeTransform(
                m_converter).map(m_cursorOutline);
        paintToolOutline(&gc, mappedOutline);
    }
}

void KisToolTransform::setFunctionalCursor()
{
    if (overrideCursorIfNotEditable()) {
        return;
    }

    if (!m_strokeId) {
        useTransformToolCursor(canvas(), {TransformCursorKind::PointingHand});
    } else if (m_strokeId && m_transaction.rootNodes().isEmpty()) {
        // we are in the middle of stroke initialization
        useTransformToolCursor(canvas(), {TransformCursorKind::Wait});
    } else {
        useTransformToolCursor(canvas(), currentStrategy()->getCurrentCursor());
    }
}

void KisToolTransform::cursorOutlineUpdateRequested(const PkPointF &imagePos)
{
    PkRect canvasUpdateRect;

    if (!m_cursorOutline.isEmpty()) {
        canvasUpdateRect = m_converter->
            imageToDocument(m_cursorOutline.boundingRect()).toAlignedRect();
    }

    m_cursorOutline = currentStrategy()->
        getCursorOutline().translated(imagePos);

    if (!m_cursorOutline.isEmpty()) {
        canvasUpdateRect |=
            m_converter->
            imageToDocument(m_cursorOutline.boundingRect()).toAlignedRect();
    }

    if (!canvasUpdateRect.isEmpty()) {
        // grow rect a bit to follow interpolation fuzziness
        canvasUpdateRect = kisGrowRect(canvasUpdateRect, 2);
        canvas()->updateCanvas(canvasUpdateRect);
    }
}

void KisToolTransform::beginActionImpl(KoPointerEvent *event, bool usePrimaryAction, KisTool::AlternateAction action)
{
    if (!nodeEditable()) {
        event->ignore();
        return;
    }

    if (!m_strokeId) {
        startStroke(m_currentArgs.mode(), action == KisTool::ChangeSize);
    } else if (!m_transaction.rootNodes().isEmpty()) {
        bool result = false;

        if (usePrimaryAction) {
            result = currentStrategy()->beginPrimaryAction(event);
        } else {
            result = currentStrategy()->beginAlternateAction(event, action);
        }

        if (result) {
            setMode(KisTool::PAINT_MODE);
        }
    }

    m_actuallyMoveWhileSelected = false;

    outlineChanged();
}

void KisToolTransform::continueActionImpl(KoPointerEvent *event, bool usePrimaryAction, KisTool::AlternateAction action)
{
    if (mode() != KisTool::PAINT_MODE) return;
    if (m_transaction.rootNodes().isEmpty()) return;

    m_actuallyMoveWhileSelected = true;

    if (usePrimaryAction) {
        currentStrategy()->continuePrimaryAction(event);
    } else {
        currentStrategy()->continueAlternateAction(event, action);
    }

    updateOptionWidget();
    outlineChanged();
}

void KisToolTransform::endActionImpl(KoPointerEvent *event, bool usePrimaryAction, KisTool::AlternateAction action)
{
    if (mode() != KisTool::PAINT_MODE) return;

    setMode(KisTool::HOVER_MODE);

    if (m_actuallyMoveWhileSelected ||
        currentStrategy()->acceptsClicks()) {

        bool result = false;

        if (usePrimaryAction) {
            result = currentStrategy()->endPrimaryAction(event);
        } else {
            result = currentStrategy()->endAlternateAction(event, action);
        }

        if (result) {
            commitChanges();
        }

        outlineChanged();
    }

    updateOptionWidget();
    updateApplyResetAvailability();
}

void KisToolTransform::beginPrimaryAction(KoPointerEvent *event)
{
    beginActionImpl(event, true, KisTool::NONE);
}

void KisToolTransform::continuePrimaryAction(KoPointerEvent *event)
{
    continueActionImpl(event, true, KisTool::NONE);
}

void KisToolTransform::endPrimaryAction(KoPointerEvent *event)
{
    endActionImpl(event, true, KisTool::NONE);
}

void KisToolTransform::activatePrimaryAction()
{
    currentStrategy()->activatePrimaryAction();
    setFunctionalCursor();
}

void KisToolTransform::deactivatePrimaryAction()
{
    currentStrategy()->deactivatePrimaryAction();
}

void KisToolTransform::activateAlternateAction(AlternateAction action)
{
    currentStrategy()->activateAlternateAction(action);
    setFunctionalCursor();
}

void KisToolTransform::deactivateAlternateAction(AlternateAction action)
{
    currentStrategy()->deactivateAlternateAction(action);
}

void KisToolTransform::beginAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    beginActionImpl(event, false, action);
}

void KisToolTransform::continueAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    continueActionImpl(event, false, action);
}

void KisToolTransform::endAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    endActionImpl(event, false, action);
}

void KisToolTransform::mousePressEvent(KoPointerEvent *event)
{
    // When using touch drawing, we only ever receive move events after the
    // finger has pressed down. This confuses the strategies greatly, since they
    // expect to receive a hover to tell which anchor the user wants to
    // manipulate or similar. So in this case, we send an artificial hover.
    if (event->isTouchEvent() && this->mode() != KisTool::PAINT_MODE) {
        currentStrategy()->hoverActionCommon(event);
        setFunctionalCursor();
    }
    KisTool::mousePressEvent(event);
}

void KisToolTransform::mouseMoveEvent(KoPointerEvent *event)
{
    PkPointF mousePos = m_converter->documentToImage(event->point);

    cursorOutlineUpdateRequested(mousePos);

    if (this->mode() != KisTool::PAINT_MODE) {
        currentStrategy()->hoverActionCommon(event);
        setFunctionalCursor();
        KisTool::mouseMoveEvent(event);
        return;
    }
}

void KisToolTransform::mouseReleaseEvent(KoPointerEvent *event)
{
    KisTool::mouseReleaseEvent(event);
}

void KisToolTransform::applyTransform()
{
    slotApplyTransform();
}

void KisToolTransform::setNextActivationTransformMode(KisToolTransform::TransformToolMode mode)
{
    nextActivationTransformMode = mode;
}

KisToolTransform::TransformToolMode KisToolTransform::transformMode() const
{
    TransformToolMode mode = FreeTransformMode;

    switch (m_currentArgs.mode())
    {
    case ToolTransformArgs::FREE_TRANSFORM:
        mode = FreeTransformMode;
        break;
    case ToolTransformArgs::WARP:
        mode = WarpTransformMode;
        break;
    case ToolTransformArgs::CAGE:
        mode = CageTransformMode;
        break;
    case ToolTransformArgs::LIQUIFY:
        mode = LiquifyTransformMode;
        break;
    case ToolTransformArgs::PERSPECTIVE_4POINT:
        mode = PerspectiveTransformMode;
        break;
    case ToolTransformArgs::MESH:
        mode = MeshTransformMode;
        break;
    default:
        KIS_ASSERT_RECOVER_NOOP(0 && "unexpected transform mode");
    }

    return mode;
}

double KisToolTransform::translateX() const
{
    return m_currentArgs.transformedCenter().x();
}

double KisToolTransform::translateY() const
{
    return m_currentArgs.transformedCenter().y();
}

double KisToolTransform::rotateX() const
{
    return m_currentArgs.aX();
}

double KisToolTransform::rotateY() const
{
    return m_currentArgs.aY();
}

double KisToolTransform::rotateZ() const
{
    return m_currentArgs.aZ();
}

double KisToolTransform::scaleX() const
{
    return m_currentArgs.scaleX();
}

double KisToolTransform::scaleY() const
{
    return m_currentArgs.scaleY();
}

double KisToolTransform::shearX() const
{
    return m_currentArgs.shearX();
}

double KisToolTransform::shearY() const
{
    return m_currentArgs.shearY();
}

KisToolTransform::WarpType KisToolTransform::warpType() const
{
    switch(m_currentArgs.warpType()) {
    case KisWarpTransformWorker::AFFINE_TRANSFORM:
        return AffineWarpType;
    case KisWarpTransformWorker::RIGID_TRANSFORM:
        return RigidWarpType;
    case KisWarpTransformWorker::SIMILITUDE_TRANSFORM:
        return SimilitudeWarpType;
    default:
        return RigidWarpType;
    }
}

double KisToolTransform::warpFlexibility() const
{
    return m_currentArgs.alpha();
}

int KisToolTransform::warpPointDensity() const
{
    return m_currentArgs.numPoints();
}

ToolTransformArgs::TransformMode KisToolTransform::toArgsMode(KisToolTransform::TransformToolMode toolMode)
{
    ToolTransformArgs::TransformMode mode = ToolTransformArgs::FREE_TRANSFORM;

    switch (toolMode) {
    case FreeTransformMode:
        mode = ToolTransformArgs::FREE_TRANSFORM;
        break;
    case WarpTransformMode:
        mode = ToolTransformArgs::WARP;
        break;
    case CageTransformMode:
        mode = ToolTransformArgs::CAGE;
        break;
    case LiquifyTransformMode:
        mode = ToolTransformArgs::LIQUIFY;
        break;
    case PerspectiveTransformMode:
        mode = ToolTransformArgs::PERSPECTIVE_4POINT;
        break;
    case MeshTransformMode:
        mode = ToolTransformArgs::MESH;
        break;
    default:
        KIS_ASSERT_RECOVER_NOOP(0 && "unexpected transform mode");
    }

    return mode;
}

void KisToolTransform::setTransformMode(KisToolTransform::TransformToolMode newMode)
{
    ToolTransformArgs::TransformMode mode = toArgsMode(newMode);

    if( mode != m_currentArgs.mode() ) {
        // Was routed through the options panel's mode buttons, each of which
        // just did sigResetTransform(<mode>) connected to
        // slotResetTransform(). Call it directly now that the panel is gone.
        slotResetTransform(mode);

        transformModeChanged();
    }
}

void KisToolTransform::setRotateX( double rotation )
{
    m_currentArgs.setAX( rotation );
}

void KisToolTransform::setRotateY( double rotation )
{
    m_currentArgs.setAY( rotation );
}

void KisToolTransform::setRotateZ( double rotation )
{
    m_currentArgs.setAZ( rotation );
}

void KisToolTransform::setWarpType( KisToolTransform::WarpType type )
{
    switch( type ) {
    case RigidWarpType:
        m_currentArgs.setWarpType(KisWarpTransformWorker::RIGID_TRANSFORM);
        break;
    case AffineWarpType:
        m_currentArgs.setWarpType(KisWarpTransformWorker::AFFINE_TRANSFORM);
        break;
    case SimilitudeWarpType:
        m_currentArgs.setWarpType(KisWarpTransformWorker::SIMILITUDE_TRANSFORM);
        break;
    default:
        break;
    }
}

void KisToolTransform::setWarpFlexibility( double flexibility )
{
    m_currentArgs.setAlpha( flexibility );
}

void KisToolTransform::setWarpPointDensity( int density )
{
    // Was forwarded to the options panel's slotSetWarpDensity(), which just
    // called KisTransformUtils::setDefaultWarpPoints() -- call it directly.
    ToolTransformArgs *config = m_transaction.currentConfig();
    KisTransformUtils::setDefaultWarpPoints(density, &m_transaction, config);
    slotUiChangedConfig(true);
}

void KisToolTransform::initTransformMode(ToolTransformArgs::TransformMode mode)
{
    m_currentArgs = KisTransformUtils::resetArgsForMode(mode, m_currentArgs.filterId(), m_transaction, m_currentArgs.externalSource());
    initGuiAfterTransformMode();
}

void KisToolTransform::initGuiAfterTransformMode()
{
    currentStrategy()->externalConfigChanged();
    outlineChanged();
    updateOptionWidget();
    updateApplyResetAvailability();
    setFunctionalCursor();
}

void KisToolTransform::initThumbnailImage(KisPaintDeviceSP previewDevice)
{
    PkImage origImg;
    m_selectedPortionCache = previewDevice;

    PkTransform thumbToImageTransform;

    const int maxSize = 2000;

    PkRect srcRect(m_transaction.originalRect().toAlignedRect());
    int x, y, w, h;
    srcRect.getRect(&x, &y, &w, &h);

    if (m_selectedPortionCache) {
        if (w > maxSize || h > maxSize) {
            qreal scale = qreal(maxSize) / (w > h ? w : h);
            PkTransform scaleTransform = PkTransform::fromScale(scale, scale);

            PkRect thumbRect = scaleTransform.mapRect(m_transaction.originalRect()).toAlignedRect();

            origImg = m_selectedPortionCache->
                    createThumbnailUncached(thumbRect.width(),
                                    thumbRect.height(),
                                    srcRect, 1,
                                    KoColorConversionTransformation::internalRenderingIntent(),
                                    KoColorConversionTransformation::internalConversionFlags());
            thumbToImageTransform = scaleTransform.inverted();

        } else {
            origImg = m_selectedPortionCache->convertToQImage(0, x, y, w, h,
                                                              KoColorConversionTransformation::internalRenderingIntent(),
                                                              KoColorConversionTransformation::internalConversionFlags());
            thumbToImageTransform = PkTransform();
        }
    }

    // init both strokes since the thumbnail is initialized only once
    // during the stroke
    m_freeStrategy->setThumbnailImage(origImg, thumbToImageTransform);
    m_perspectiveStrategy->setThumbnailImage(origImg, thumbToImageTransform);
    m_warpStrategy->setThumbnailImage(origImg, thumbToImageTransform);
    m_cageStrategy->setThumbnailImage(origImg, thumbToImageTransform);
    m_liquifyStrategy->setThumbnailImage(origImg, thumbToImageTransform);
    m_meshStrategy->setThumbnailImage(origImg, thumbToImageTransform);
}

void KisToolTransform::newActivationWithExternalSource(KisPaintDeviceSP externalSource)
{
    m_externalSourceForNextActivation = externalSource;
    if (isActive()) {
        PkSet<KoShape*> dummy;
        deactivate();
        activate(dummy);
    } else {
        KoToolManager::instance()->switchToolRequested("KisToolTransform");
    }
}

void KisToolTransform::activate(const PkSet<KoShape*> &shapes)
{
    KisTool::activate(shapes);

    /// we cannot initialize the setting in the constructor, because
    /// factory() is not yet initialized, so we cannot get toolId()
    slotGlobalConfigChanged();

    if (currentNode()) {
        m_transaction = TransformTransactionProperties(PkRectF(), &m_currentArgs, KisNodeList(), {});
    }

    startStroke(toArgsMode(nextActivationTransformMode), false);
    nextActivationTransformMode = KisToolTransform::FreeTransformMode;
}

void KisToolTransform::deactivate()
{
    endStroke();
    KisCanvasInvalidation *invalidation = dynamic_cast<KisCanvasInvalidation *>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(invalidation);
    invalidation->invalidateAll();
    KisTool::deactivate();
}

bool KisToolTransform::dispatchPlatformAction(PlatformAction action, bool checked)
{
    switch (action) {
    case PlatformAction::Free: slotUpdateToFreeTransformType(); break;
    case PlatformAction::Perspective: slotUpdateToPerspectiveType(); break;
    case PlatformAction::Warp: slotUpdateToWarpType(); break;
    case PlatformAction::Cage: slotUpdateToCageType(); break;
    case PlatformAction::Liquify: slotUpdateToLiquifyType(); break;
    case PlatformAction::Mesh: slotUpdateToMeshType(); break;
    case PlatformAction::MirrorHorizontal: slotFlipHorizontal(); break;
    case PlatformAction::MirrorVertical: slotFlipVertical(); break;
    case PlatformAction::RotateClockwise: slotRotateNinetyCW(); break;
    case PlatformAction::RotateCounterClockwise: slotRotateNinetyCCW(); break;
    case PlatformAction::KeepAspectRatio: slotSetKeepAspectRatio(checked); break;
    case PlatformAction::Apply: slotApplyTransform(); break;
    case PlatformAction::Reset: slotCancelTransform(); break;
    case PlatformAction::MoveUp: slotMoveDiscreteUp(); break;
    case PlatformAction::MoveUpMore: slotMoveDiscreteUpMore(); break;
    case PlatformAction::MoveDown: slotMoveDiscreteDown(); break;
    case PlatformAction::MoveDownMore: slotMoveDiscreteDownMore(); break;
    case PlatformAction::MoveLeft: slotMoveDiscreteLeft(); break;
    case PlatformAction::MoveLeftMore: slotMoveDiscreteLeftMore(); break;
    case PlatformAction::MoveRight: slotMoveDiscreteRight(); break;
    case PlatformAction::MoveRightMore: slotMoveDiscreteRightMore(); break;
    case PlatformAction::IncreaseBrushSize: slotIncreaseBrushSize(); break;
    case PlatformAction::DecreaseBrushSize: slotDecreaseBrushSize(); break;
    }
    return true;
}

void KisToolTransform::requestUndoDuringStroke()
{
    if (!m_strokeId || m_transaction.rootNodes().isEmpty() || mode() != HOVER_MODE) return;

    if (!m_changesTracker.canUndo()) {
        cancelStroke();
    } else {
        m_changesTracker.requestUndo();
    }
}

void KisToolTransform::requestRedoDuringStroke()
{
    if (!m_strokeId || m_transaction.rootNodes().isEmpty()) return;

    if (m_changesTracker.canRedo()) {
        m_changesTracker.requestRedo();
    }
}

void KisToolTransform::requestStrokeEnd()
{
    endStroke();
}

void KisToolTransform::requestStrokeCancellation()
{
    if (m_transaction.rootNodes().isEmpty() || m_currentArgs.isIdentity()) {
        cancelStroke();
    } else {
        slotCancelTransform();
    }
}

void KisToolTransform::requestImageRecalculation()
{
    if (!m_currentlyUsingOverlayPreviewStyle && m_strokeId && !m_transaction.rootNodes().isEmpty()) {
        image()->addJob(
            m_strokeId,
            new InplaceTransformStrokeStrategy::UpdateTransformData(
                m_currentArgs,
                InplaceTransformStrokeStrategy::UpdateTransformData::PAINT_DEVICE));
    }
}

void KisToolTransform::startStroke(ToolTransformArgs::TransformMode mode, bool forceReset)
{
    KIS_ASSERT(!m_strokeId);

    KisPaintDeviceSP externalSource = m_externalSourceForNextActivation;
    m_externalSourceForNextActivation.clear();

    // set up and null checks before we do anything
    KisResourcesSnapshotSP resources =
            new KisResourcesSnapshot(image(), currentNode(), this->canvas()->resourceManager()->canvasResourcesInterface(), 0, selectedNodes(), 0);
    KisNodeList rootNodes = resources->selectedNodes();
    //Filter out any nodes that might be children of other selected nodes so they aren't used twice
    KisLayerUtils::filterMergeableNodes(rootNodes, true);
    KisSelectionSP selection = resources->activeSelection();

    m_transaction = TransformTransactionProperties(PkRectF(), &m_currentArgs, KisNodeList(), {});
    m_currentArgs = ToolTransformArgs();

    for (const KisNodeSP &currentNode : resources->selectedNodes()) {
        if (!currentNode || !currentNode->isEditable()) {
            if (currentNode && currentNode->userLocked()) {
                showTransformToolMessage(canvas(), "Cannot transform locked layers", 4000,
                                         TransformToolMessagePriority::High);
            } else if (currentNode && !currentNode->visible()) {
                showTransformToolMessage(canvas(), "Cannot transform hidden layers", 4000,
                                         TransformToolMessagePriority::High);
            } else {
                showTransformToolMessage(canvas(),
                                         "Cannot use transform tool on this set of layers", 4000,
                                         TransformToolMessagePriority::High);
            }

            return;
        }

        // some layer types cannot be transformed. Give a message and return if a user tries it
        if (currentNode->inherits("KisColorizeMask") ||
            currentNode->inherits("KisFileLayer") ||
            currentNode->inherits("KisCloneLayer")) {

            if(currentNode->inherits("KisColorizeMask")){
                showTransformToolMessage(canvas(), "Layer type cannot use the transform tool", 4000,
                                         TransformToolMessagePriority::High);
            }
            else{
                showTransformToolMessage(canvas(),
                                         "Layer type cannot use the transform tool. Use transform mask instead.",
                                         4000, TransformToolMessagePriority::High);
            }
            return;
        }

        KisNodeSP impossibleMask =
            KisLayerUtils::recursiveFindNode(currentNode,
            [currentNode] (KisNodeSP node) {
                // we can process transform masks of the first level
                if (node == currentNode || node->parent() == currentNode) return false;

                return node->inherits("KisTransformMask") && node->visible(true);
            });

        if (impossibleMask) {
            showTransformToolMessage(canvas(),
                                     "Layer has children with transform masks. Please disable them before doing transformation.",
                                     8000, TransformToolMessagePriority::High);
            return;
        }

        /**
         * When working with transform mask, selections are not
         * taken into account.
         */
        if (selection && dynamic_cast<KisTransformMask*>(currentNode.data())) {
            showTransformToolMessage(canvas(),
                                     "Selections are not used when editing transform masks ", 4000,
                                     TransformToolMessagePriority::Low);

            selection = nullptr;
        }
    }
    // Overlay preview is never used when transforming an externally provided image
    m_currentlyUsingOverlayPreviewStyle = m_preferOverlayPreviewStyle && !externalSource;

    KisStrokeStrategy *strategy = 0;

    if (m_currentlyUsingOverlayPreviewStyle) {
        TransformStrokeStrategy *transformStrategy = new TransformStrokeStrategy(mode, m_currentArgs.filterId(), forceReset, rootNodes, selection, image().data(), image().data());
        PkObject::connect(transformStrategy, &TransformStrokeStrategy::sigPreviewDeviceReady,
                          this, &KisToolTransform::slotPreviewDeviceGenerated);
        PkObject::connect(transformStrategy, &TransformStrokeStrategy::sigTransactionGenerated,
                          this, &KisToolTransform::slotTransactionGenerated);
        PkObject::connect(transformStrategy, &TransformStrokeStrategy::sigConvexHullCalculated,
                          this, &KisToolTransform::slotConvexHullCalculated);
        strategy = transformStrategy;

        // save unique identifier of the stroke so we could
        // recognize it when sigTransactionGenerated() is
        // received (theoretically, the user can start two
        // strokes at the same time, if he is quick enough)
        m_strokeStrategyCookie = transformStrategy;

    } else {
        InplaceTransformStrokeStrategy *transformStrategy = new InplaceTransformStrokeStrategy(mode, m_currentArgs.filterId(), forceReset, rootNodes, selection, externalSource, image().data(), image().data(), image()->root(), m_forceLodMode);
        PkObject::connect(transformStrategy, &InplaceTransformStrokeStrategy::sigTransactionGenerated,
                          this, &KisToolTransform::slotTransactionGenerated);
        PkObject::connect(transformStrategy, &InplaceTransformStrokeStrategy::sigConvexHullCalculated,
                          this, &KisToolTransform::slotConvexHullCalculated);
        strategy = transformStrategy;

        // save unique identifier of the stroke so we could
        // recognize it when sigTransactionGenerated() is
        // received (theoretically, the user can start two
        // strokes at the same time, if he is quick enough)
        m_strokeStrategyCookie = transformStrategy;
    }

    m_strokeId = image()->startStroke(strategy);

    if (!m_currentlyUsingOverlayPreviewStyle) {
        m_asyncUpdateHelper.initUpdateStreamLowLevel(image().data(), m_strokeId);
    }

    KIS_SAFE_ASSERT_RECOVER_NOOP(m_changesTracker.isEmpty());

    slotPreviewDeviceGenerated(nullptr);
}

void KisToolTransform::endStroke()
{
    if (!m_strokeId) return;

    if (m_currentlyUsingOverlayPreviewStyle &&
        !m_transaction.rootNodes().isEmpty() &&
        !m_currentArgs.isUnchanging()) {

        image()->addJob(m_strokeId,
                        new TransformStrokeStrategy::TransformAllData(m_currentArgs));
    }

    if (m_asyncUpdateHelper.isActive()) {
        m_asyncUpdateHelper.endUpdateStream();
    }

    image()->endStroke(m_strokeId);

    m_strokeStrategyCookie = 0;
    m_strokeId.clear();
    m_changesTracker.reset();
    m_transaction = TransformTransactionProperties(PkRectF(), &m_currentArgs, KisNodeList(), {});
    outlineChanged();
}

void KisToolTransform::slotTransactionGenerated(TransformTransactionProperties transaction, ToolTransformArgs args, void *strokeStrategyCookie)
{
    if (!m_strokeId || strokeStrategyCookie != m_strokeStrategyCookie) return;

    if (transaction.transformedNodes().isEmpty() ||
        transaction.originalRect().isEmpty()) {

        showTransformToolMessage(canvas(), "Cannot transform empty layer ", 1000,
                                 TransformToolMessagePriority::Medium);

        cancelStroke();
        return;
    }

    m_transaction = transaction;
    m_currentArgs = args;
    m_transaction.setCurrentConfigLocation(&m_currentArgs);

    if (!m_currentlyUsingOverlayPreviewStyle) {
        m_asyncUpdateHelper.startUpdateStreamLowLevel();
    }

    KIS_SAFE_ASSERT_RECOVER_NOOP(m_changesTracker.isEmpty());
    commitChanges();

    initGuiAfterTransformMode();

    if (m_transaction.hasInvisibleNodes()) {
        showTransformToolMessage(canvas(),
                                 "Invisible sublayers will also be transformed. Lock layers if you do not want them to be transformed ",
                                 4000, TransformToolMessagePriority::Low);
    }
}

void KisToolTransform::slotPreviewDeviceGenerated(KisPaintDeviceSP device)
{
    if (device && device->exactBounds().isEmpty()) {
        showTransformToolMessage(canvas(), "Cannot transform empty layer ", 1000,
                                 TransformToolMessagePriority::Medium);

        cancelStroke();
    } else {
        initThumbnailImage(device);
        initGuiAfterTransformMode();
    }
}

void KisToolTransform::cancelStroke()
{
    if (!m_strokeId) return;

    if (m_asyncUpdateHelper.isActive()) {
        m_asyncUpdateHelper.cancelUpdateStream();
    }

    image()->cancelStroke(m_strokeId);
    m_strokeStrategyCookie = 0;
    m_strokeId.clear();
    m_changesTracker.reset();
    m_transaction = TransformTransactionProperties(PkRectF(), &m_currentArgs, KisNodeList(), {});
    outlineChanged();
}

void KisToolTransform::commitChanges()
{
    if (!m_strokeId || m_transaction.rootNodes().isEmpty()) return;

    m_changesTracker.commitConfig(toQShared(m_currentArgs.clone()));
}

void KisToolTransform::slotTrackerChangedConfig(KisToolChangesTrackerDataSP status)
{
    const ToolTransformArgs *newArgs = dynamic_cast<const ToolTransformArgs*>(status.data());
    KIS_SAFE_ASSERT_RECOVER_RETURN(newArgs);

    *m_transaction.currentConfig() = *newArgs;

    slotUiChangedConfig(true);
    updateOptionWidget();
}

void KisToolTransform::updateOptionWidget()
{
    updateTransformToolOptions(canvas(), currentNode() != nullptr, m_currentArgs);
}

void KisToolTransform::updateApplyResetAvailability()
{
    setTransformToolApplyResetEnabled(canvas(), !m_currentArgs.isIdentity());
}

void KisToolTransform::slotUiChangedConfig(bool needsPreviewRecalculation)
{
    if (mode() == KisTool::PAINT_MODE) return;

    if (needsPreviewRecalculation) {
        currentStrategy()->externalConfigChanged();
    }

    if (m_currentArgs.mode() == ToolTransformArgs::LIQUIFY) {
        m_currentArgs.saveLiquifyTransformMode();
    }

    outlineChanged();
    updateApplyResetAvailability();
}

void KisToolTransform::slotApplyTransform()
{
    useTransformToolCursor(canvas(), {TransformCursorKind::Wait});
    endStroke();
}

void KisToolTransform::slotResetTransform(ToolTransformArgs::TransformMode mode)
{
    ToolTransformArgs *config = m_transaction.currentConfig();
    const ToolTransformArgs::TransformMode previousMode = config->mode();
    config->setMode(mode);

    if (mode == ToolTransformArgs::WARP) {
        config->setWarpCalculation(KisWarpTransformWorker::WarpCalculation::GRID);
    }

    if (!m_strokeId || m_transaction.rootNodes().isEmpty()) return;

    if (m_currentArgs.continuedTransform()) {
        ToolTransformArgs::TransformMode savedMode = m_currentArgs.mode();

        /**
         * Our reset transform button can be used for two purposes:
         *
         * 1) Reset current transform to the initial one, which was
         *    loaded from the previous user action.
         *
         * 2) Reset transform frame to infinity when the frame is unchanged
         */

        const bool transformDiffers = !m_currentArgs.continuedTransform()->isSameMode(m_currentArgs);

        if (transformDiffers &&
            m_currentArgs.continuedTransform()->mode() == savedMode) {

            m_currentArgs.restoreContinuedState();
            initGuiAfterTransformMode();
            slotEditingFinished();

        } else {
            cancelStroke();
            startStroke(savedMode, true);

            KIS_ASSERT_RECOVER_NOOP(!m_currentArgs.continuedTransform());
        }
    } else {
        if (!KisTransformUtils::shouldRestartStrokeOnModeChange(previousMode,
                                                                m_currentArgs.mode(),
                                                                m_transaction.transformedNodes())) {
            initTransformMode(m_currentArgs.mode());
            slotEditingFinished();

        } else {
            cancelStroke();
            startStroke(m_currentArgs.mode(), true);

        }
    }
}

void KisToolTransform::slotCancelTransform()
{
    slotResetTransform(m_transaction.currentConfig()->mode());
}

void KisToolTransform::slotRestartTransform()
{
    if (!m_strokeId || m_transaction.rootNodes().isEmpty()) return;

    KisNodeSP root = m_transaction.rootNodes()[0];
    KIS_ASSERT_RECOVER_RETURN(root); // the stroke is guaranteed to be started by an 'if' above

    ToolTransformArgs savedArgs(m_currentArgs);
    cancelStroke();
    startStroke(savedArgs.mode(), true);
}

void KisToolTransform::slotRestartAndContinueTransform()
{
    if (!m_strokeId || m_transaction.rootNodes().isEmpty()) return;

    KisNodeSP root = m_transaction.rootNodes()[0];
    KIS_ASSERT_RECOVER_RETURN(root); // the stroke is guaranteed to be started by an 'if' above

    ToolTransformArgs savedArgs(m_currentArgs);
    endStroke();
    startStroke(savedArgs.mode(), false);
}

void KisToolTransform::slotEditingFinished()
{
    commitChanges();
}

// The following five slots are ported from the deleted options panel's
// KisToolTransformConfigWidget::slotFlipX/slotFlipY/slotRotateCW/
// slotRotateCCW/slotSetKeepAspectRatio. They back the "extra context click
// options" (mirrorHorizontalAction/mirrorVerticalAction/rotateNinetyCWAction/
// rotateNinetyCCWAction/keepAspectRatioAction) added to popupActionsMenu()
// when free transform is active -- a context-menu entry point independent of
// the panel, so it must keep working after the panel is deleted.
void KisToolTransform::slotFlipHorizontal()
{
    ToolTransformArgs *config = m_transaction.currentConfig();

    {
        KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
        config->setScaleX(config->scaleX() * -1);
    }

    slotUiChangedConfig(true);
    slotEditingFinished();
}

void KisToolTransform::slotFlipVertical()
{
    ToolTransformArgs *config = m_transaction.currentConfig();

    {
        KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
        config->setScaleY(config->scaleY() * -1);
    }

    slotUiChangedConfig(true);
    slotEditingFinished();
}

void KisToolTransform::slotRotateNinetyCW()
{
    ToolTransformArgs *config = m_transaction.currentConfig();

    {
        KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
        config->setAZ(normalizeAngle(config->aZ() + M_PI_2));
    }

    slotUiChangedConfig(true);
    slotEditingFinished();
}

void KisToolTransform::slotRotateNinetyCCW()
{
    ToolTransformArgs *config = m_transaction.currentConfig();

    {
        KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
        config->setAZ(normalizeAngle(config->aZ() - M_PI_2));
    }

    slotUiChangedConfig(true);
    slotEditingFinished();
}

void KisToolTransform::slotSetKeepAspectRatio(bool value)
{
    ToolTransformArgs *config = m_transaction.currentConfig();
    config->setKeepAspectRatio(value);

    if (value) {
        // Cache the current scaleX/scaleY ratio so setScaleX()/setScaleY()
        // can keep the other axis in sync while it is locked.
        m_scaleRatio = config->scaleY() != 0 ? config->scaleX() / config->scaleY() : 1.0;
    }

    slotUiChangedConfig(true);
}

void KisToolTransform::slotMoveDiscreteUp()
{
    setTranslateY(translateY()-1.0);
}

void KisToolTransform::slotMoveDiscreteUpMore()
{
    setTranslateY(translateY()-10.0);
}

void KisToolTransform::slotMoveDiscreteDown()
{
    setTranslateY(translateY()+1.0);
}

void KisToolTransform::slotMoveDiscreteDownMore()
{
    setTranslateY(translateY()+10.0);
}

void KisToolTransform::slotMoveDiscreteLeft()
{
    setTranslateX(translateX()-1.0);
}

void KisToolTransform::slotMoveDiscreteLeftMore()
{
    setTranslateX(translateX()-10.0);
}

void KisToolTransform::slotMoveDiscreteRight()
{
    setTranslateX(translateX()+1.0);
}

void KisToolTransform::slotMoveDiscreteRightMore()
{
    setTranslateX(translateX()+10.0);
}

void KisToolTransform::slotIncreaseBrushSize()
{
    currentStrategy()->increaseBrushSize(canvas());
}

void KisToolTransform::slotDecreaseBrushSize()
{
    currentStrategy()->decreaseBrushSize(canvas());
}

void KisToolTransform::slotUpdateToWarpType()
{
    setTransformMode(KisToolTransform::TransformToolMode::WarpTransformMode);
}

void KisToolTransform::slotUpdateToPerspectiveType()
{
    setTransformMode(KisToolTransform::TransformToolMode::PerspectiveTransformMode);
}

void KisToolTransform::slotUpdateToFreeTransformType()
{
    setTransformMode(KisToolTransform::TransformToolMode::FreeTransformMode);
}

void KisToolTransform::slotUpdateToLiquifyType()
{
    setTransformMode(KisToolTransform::TransformToolMode::LiquifyTransformMode);
}

void KisToolTransform::slotUpdateToMeshType()
{
    setTransformMode(KisToolTransform::TransformToolMode::MeshTransformMode);
}

void KisToolTransform::slotUpdateToCageType()
{
    setTransformMode(KisToolTransform::TransformToolMode::CageTransformMode);
}

// Ported from the deleted options panel's KisToolTransformConfigWidget::
// slotSetShearY/slotSetShearX/slotSetScaleY/slotSetScaleX. Values are in the
// panel's original percent-like convention (e.g. 100.0 == no scale), divided
// by 100 to get the ratio ToolTransformArgs stores -- this is why scaleX()/
// scaleY() (which return the raw ratio) and setScaleX()/setScaleY() (which
// take this percent-like value) don't share units; that mismatch predates
// this change and is preserved as-is, not fixed here.
void KisToolTransform::setShearY(double shear)
{
    ToolTransformArgs *config = m_transaction.currentConfig();
    KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
    config->setShearY(shear / 100.);
    slotUiChangedConfig(true);
    slotEditingFinished();
}

void KisToolTransform::setShearX(double shear)
{
    ToolTransformArgs *config = m_transaction.currentConfig();
    KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
    config->setShearX(shear / 100.);
    slotUiChangedConfig(true);
    slotEditingFinished();
}

void KisToolTransform::setScaleY(double scale)
{
    ToolTransformArgs *config = m_transaction.currentConfig();

    {
        KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
        config->setScaleY(scale / 100.);
    }

    if (config->keepAspectRatio()) {
        const double calculatedValue = m_scaleRatio * scale;
        KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
        config->setScaleX(calculatedValue / 100.);
    }

    slotUiChangedConfig(true);
    slotEditingFinished();
}

void KisToolTransform::setScaleX(double scale)
{
    ToolTransformArgs *config = m_transaction.currentConfig();

    {
        KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
        config->setScaleX(scale / 100.);
    }

    if (config->keepAspectRatio()) {
        const double calculatedValue = m_scaleRatio != 0 ? scale / m_scaleRatio : scale;
        KisTransformUtils::AnchorHolder keeper(config->transformAroundRotationCenter(), config);
        config->setScaleY(calculatedValue / 100.);
    }

    slotUiChangedConfig(true);
    slotEditingFinished();
}

void KisToolTransform::setTranslateY(double translation)
{
    TransformToolMode mode = transformMode();

    if (m_strokeId && (mode == FreeTransformMode || mode == PerspectiveTransformMode)) {
        m_currentArgs.setTransformedCenter(PkPointF(translateX(), translation));
        currentStrategy()->externalConfigChanged();
        updateOptionWidget();
        outlineChanged();
    }
}

void KisToolTransform::setTranslateX(double translation)
{
    TransformToolMode mode = transformMode();

    if (m_strokeId && (mode == FreeTransformMode || mode == PerspectiveTransformMode)) {
        m_currentArgs.setTransformedCenter(PkPointF(translation, translateY()));
        currentStrategy()->externalConfigChanged();
        updateOptionWidget();
        outlineChanged();
    }
}

void KisToolTransformFactory::activateSubtool(KisToolTransform::TransformToolMode mode)
{
    KoToolManager *toolManager = KoToolManager::instance();

    KoCanvasController *canvasController = toolManager->activeCanvasController();
    if (!canvasController) return;
    KoCanvasBase *canvas = canvasController->canvas();
    if (!canvas) return;

    KoToolBase *tool = toolManager->toolById(canvas, id());
    KIS_SAFE_ASSERT_RECOVER_RETURN(tool);
    KisToolTransform *transformTool = dynamic_cast<KisToolTransform*>(tool);
    KIS_SAFE_ASSERT_RECOVER_RETURN(transformTool);

    if (toolManager->activeToolId() == id()) {
        // Transform tool is already active, switch the current mode
        transformTool->setTransformMode(mode);
    } else {
        // Works like KoToolFactoryBase::activateTool, but tells the tool beforehand which initial transform mode to use
        transformTool->setNextActivationTransformMode(mode);
        toolManager->switchToolRequested(id());
    }
}
