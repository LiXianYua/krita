/*
 *  SPDX-FileCopyrightText: 2006, 2010 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <pk/container/PkSet.h>
#include <pk/geometry/PkPoint.h>
#include <pk/geometry/PkPolygon.h>
#include <pk/geometry/PkRect.h>
#include <pk/geometry/PkTransform.h>

#include "kis_tool.h"

#include <PkObject.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkTransform.h>

#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KoColor.h>
#include <KoCanvasBase.h>
#include <KoCanvasController.h>
#include <KoToolBase.h>
#include <KoID.h>
#include <KoPointerEvent.h>
#include <KoViewConverter.h>
#include <KoSelection.h>
#include <resources/KoAbstractGradient.h>
#include <KoSnapGuide.h>

#include <kis_selection.h>
#include <kis_image.h>
#include <kis_group_layer.h>
#include <kis_adjustment_layer.h>
#include <kis_mask.h>
#include <kis_paint_layer.h>
#include <kis_painter.h>
#include <brushengine/kis_paintop_preset.h>
#include <brushengine/kis_paintop_settings.h>
#include <resources/KoPattern.h>
#include "filter/kis_filter_configuration.h"
#include "kis_config_notifier.h"
#include <kis_selection_mask.h>
#include "kis_resources_snapshot.h"
#include <kis_layer_utils.h>
#include <kis_transaction.h>
#include <kis_command_utils.h>
#include <kis_processing_applicator.h>
#include <KisAnimAutoKey.h>
#include <KisOptimizedBrushOutline.h>
#include <KisCanvasFeedback.h>
#include <KisCanvasToolServices.h>

struct Q_DECL_HIDDEN KisTool::Private {
    KisCanvasCursorToken cursor; // Host-owned cursor shown on activation.

    // From the canvas resources
    KoPatternSP currentPattern;
    KoAbstractGradientSP currentGradient;
    KoColor currentFgColor;
    KoColor currentBgColor;
    float currentExposure{1.0};
    KisFilterConfigurationSP currentGenerator;
    ToolMode m_mode{HOVER_MODE};
    bool m_isActive{false};
};

namespace {



PkString nodeEditableMessage(KisNodeSP node, bool blockedNoIndirectPainting)
{
    PkString message;
    if (!node->isEditable(true) || blockedNoIndirectPainting) {
        if (!node->visible() && node->userLocked()) {
            message = PkString("Layer is locked and invisible.");
        } else if (node->userLocked()) {
            message = PkString("Layer is locked.");
        } else if (!node->visible()) {
            message = PkString("Layer is invisible.");
        } else if (blockedNoIndirectPainting) {
            message = PkString("Layer can be painted in Wash Mode only.");
        } else {
            message = PkString("Group not editable.");
        }
    }
    return message;
}

bool clearImage(KisImageSP image, KisNodeList nodes, KisSelectionSP selection)
{
    KisNodeList masks;

    for (KisNodeSP node : nodes) {
        if (node->inherits("KisMask")) {
            masks.append(node);
        }
    }

    KisLayerUtils::filterMergeableNodes(nodes);
    nodes.append(masks);

    if (nodes.isEmpty()) {
        return false;
    }

    KisProcessingApplicator applicator(image, 0, KisProcessingApplicator::NONE,
                                       KisImageSignalVector(), kundo2_text("Clear"));

    for (KisNodeSP node : nodes) {
        KisLayerUtils::recursiveApplyNodes(node, [&applicator, selection, masks] (KisNodeSP node) {
            if (node->inherits("KisMask") && !masks.contains(node)) {
                return;
            }

            if (node->hasEditablePaintDevice()) {
                KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
                    kundo2_text("Clear"), [node, selection] () {
                        KisPaintDeviceSP device = node->paintDevice();
                        std::unique_ptr<KisCommandUtils::CompositeCommand> parentCommand(
                            new KisCommandUtils::CompositeCommand());

                        KUndo2Command *autoKeyframeCommand = KisAutoKey::tryAutoCreateDuplicatedFrame(device);
                        if (autoKeyframeCommand) {
                            parentCommand->addCommand(autoKeyframeCommand);
                        }

                        KisTransaction transaction(kundo2_text_raw("internal-clear-command"), device);
                        PkRect dirtyRect;
                        if (selection) {
                            dirtyRect = selection->selectedRect();
                            device->clearSelection(selection);
                        } else {
                            dirtyRect = device->extent();
                            device->clear();
                        }

                        device->setDirty(dirtyRect);
                        parentCommand->addCommand(transaction.endAndTake());
                        return parentCommand.release();
                    });
                applicator.applyCommand(cmd, KisStrokeJobData::CONCURRENT);
            }
        });
    }

    applicator.end();
    return true;
}

} // namespace

KisTool::KisTool(KoCanvasBase * canvas, KisCanvasCursorToken cursor)
    : KoToolBase(canvas)
    , d(new Private)
{
    d->cursor = cursor;

    KisConfigNotifier *notifier = KisConfigNotifier::instance();
    PkObject::connect(
        notifier, &KisConfigNotifier::configChanged, this,
        [this]() { resetCursorStyle(); });
    PkObject::connect(this, &KisTool::isActiveChanged,
                      this, [this](bool) { resetCursorStyle(); });
}

KisTool::~KisTool()
{
    delete d;
}

void KisTool::activate(const PkSet<KoShape*> &shapes)
{
    KoToolBase::activate(shapes);

    resetCursorStyle();

    if (!canvas()) return;
    if (!canvas()->resourceManager()) return;


    d->currentFgColor = canvas()->resourceManager()->resource(KoCanvasResource::ForegroundColor).value<KoColor>();
    d->currentBgColor = canvas()->resourceManager()->resource(KoCanvasResource::BackgroundColor).value<KoColor>();

    if (canvas()->resourceManager()->hasResource(KoCanvasResource::CurrentPattern)) {
        d->currentPattern = canvas()->resourceManager()->resource(KoCanvasResource::CurrentPattern).value<KoPatternSP>();
    }

    if (canvas()->resourceManager()->hasResource(KoCanvasResource::CurrentGradient)) {
        d->currentGradient = canvas()->resourceManager()->resource(KoCanvasResource::CurrentGradient).value<KoAbstractGradientSP>();
    }

    KisPaintOpPresetSP preset = canvas()->resourceManager()->resource(KoCanvasResource::CurrentPaintOpPreset).value<KisPaintOpPresetSP>();
    if (preset && preset->settings()) {
        preset->settings()->activate();
    }

    if (canvas()->resourceManager()->hasResource(KoCanvasResource::HdrExposure)) {
        d->currentExposure = static_cast<float>(canvas()->resourceManager()->resource(KoCanvasResource::HdrExposure).toDouble());
    }

    if (canvas()->resourceManager()->hasResource(KoCanvasResource::CurrentGeneratorConfiguration)) {
        d->currentGenerator = canvas()->resourceManager()->resource(KoCanvasResource::CurrentGeneratorConfiguration).value<KisFilterConfiguration*>();
    }

    d->m_isActive = true;
    isActiveChanged(true);
}

void KisTool::deactivate()
{
    d->m_isActive = false;
    isActiveChanged(false);

    KoToolBase::deactivate();
}

void KisTool::canvasResourceChanged(int key, const PkVariant & v)
{
    switch (key) {
    case(KoCanvasResource::ForegroundColor):
        d->currentFgColor = v.value<KoColor>();
        break;
    case(KoCanvasResource::BackgroundColor):
        d->currentBgColor = v.value<KoColor>();
        break;
    case(KoCanvasResource::CurrentPattern):
        d->currentPattern = v.value<KoPatternSP>();
        break;
    case(KoCanvasResource::CurrentGradient):
        d->currentGradient = v.value<KoAbstractGradientSP>();
        break;
    case(KoCanvasResource::HdrExposure):
        d->currentExposure = static_cast<float>(v.toDouble());
        break;
    case(KoCanvasResource::CurrentGeneratorConfiguration):
        d->currentGenerator = static_cast<KisFilterConfiguration*>(v.value<void *>());
        break;
    case(KoCanvasResource::CurrentKritaNode):
        resetCursorStyle();
        break;
    default:
        break; // Do nothing
    };
}

void KisTool::updateSettingsViews()
{
}

PkPointF KisTool::widgetCenterInWidgetPixels()
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    Q_ASSERT(services);
    return services->toolWidgetCenterInWidgetPixels();
}

PkPointF KisTool::convertDocumentToWidget(const PkPointF& pt)
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    Q_ASSERT(services);
    return services->toolDocumentToWidget(pt);
}

PkPointF KisTool::convertToPixelCoord(KoPointerEvent *e)
{
    if (!image())
        return e->point;

    return image()->documentToPixel(e->point);
}

PkPointF KisTool::convertToPixelCoord(const PkPointF& pt)
{
    if (!image())
        return pt;

    return image()->documentToPixel(pt);
}

PkPointF KisTool::convertToPixelCoordAndAlignOnWidget(const PkPointF &pt)
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    KIS_ASSERT(services);
    return services->toolDocumentToAlignedImagePixel(pt);
}

PkPointF KisTool::convertToPixelCoordAndSnap(KoPointerEvent *e, const PkPointF &offset, bool useModifiers)
{
    if (!image())
        return e->point;

    KoSnapGuide *snapGuide = canvas()->snapGuide();
    const Pk::KeyboardModifiers modifiers = useModifiers
        ? Pk::KeyboardModifiers(static_cast<int>(e->modifiers()))
        : Pk::KeyboardModifiers();
    PkPointF pos = snapGuide->snap(e->point, offset, modifiers);

    return image()->documentToPixel(pos);
}

PkPointF KisTool::convertToPixelCoordAndSnap(const PkPointF& pt, const PkPointF &offset)
{
    if (!image())
        return pt;

    KoSnapGuide *snapGuide = canvas()->snapGuide();
    PkPointF pos = snapGuide->snap(pt, offset, Pk::KeyboardModifiers());

    return image()->documentToPixel(pos);
}

PkPoint KisTool::convertToImagePixelCoordFloored(KoPointerEvent *e)
{
    if (!image())
        return e->point.toPoint();

    return image()->documentToImagePixelFloored(e->point);
}

PkPointF KisTool::viewToPixel(const PkPointF &viewCoord) const
{
    if (!image())
        return viewCoord;

    return image()->documentToPixel(
        canvas()->viewConverter()->viewToDocument(viewCoord));
}

PkRectF KisTool::convertToPt(const PkRectF &rect)
{
    if (!image())
        return rect;
    PkRectF r;
    //We add 1 in the following to the extreme coords because a pixel always has size
    r.setCoords(int(rect.left()) / image()->xRes(), int(rect.top()) / image()->yRes(),
                int(rect.right()) / image()->xRes(), int( rect.bottom()) / image()->yRes());
    return r;
}

qreal KisTool::convertToPt(qreal value)
{
    const qreal avgResolution = 0.5 * (image()->xRes() + image()->yRes());
    return value / avgResolution;
}

PkPointF KisTool::pixelToView(const PkPointF &pixelCoord) const
{
    if (!image())
        return pixelCoord;
    PkPointF documentCoord = image()->pixelToDocument(pixelCoord);
    return canvas()->viewConverter()->documentToView(documentCoord);
}

PkRectF KisTool::pixelToView(const PkRectF &pixelRect) const
{
    if (!image()) {
        return pixelRect;
    }
    PkPointF topLeft = pixelToView(pixelRect.topLeft());
    PkPointF bottomRight = pixelToView(pixelRect.bottomRight());
    return {topLeft, bottomRight};
}

PkPainterPath KisTool::pixelToView(const PkPainterPath &pixelPolygon) const
{
    PkTransform matrix;
    qreal zoomX, zoomY;
    canvas()->viewConverter()->zoom(&zoomX, &zoomY);
    matrix.scale(zoomX/image()->xRes(), zoomY/ image()->yRes());
    return matrix.map(pixelPolygon);
}

KisOptimizedBrushOutline KisTool::pixelToView(const KisOptimizedBrushOutline &path) const
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    KIS_ASSERT(services);
    return path.mapped(services->toolImageToViewTransform());
}


PkPolygonF KisTool::pixelToView(const PkPolygonF &pixelPath) const
{
    PkTransform matrix;
    qreal zoomX, zoomY;
    canvas()->viewConverter()->zoom(&zoomX, &zoomY);
    matrix.scale(zoomX/image()->xRes(), zoomY/ image()->yRes());
    return matrix.map(pixelPath);
}

void KisTool::updateCanvasPixelRect(const PkRectF &pixelRect)
{
    canvas()->updateCanvas(convertToPt(pixelRect));
}

void KisTool::updateCanvasViewRect(const PkRectF &viewRect)
{
    canvas()->updateCanvas(canvas()->viewConverter()->viewToDocument(viewRect));
}

KisImageWSP KisTool::image() const
{
    if (KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas())) {
        return services->toolImage();
    }

    return 0;

}

KisCanvasCursorToken KisTool::cursor() const
{
    return d->cursor;
}

KoPatternSP KisTool::currentPattern()
{
    return d->currentPattern;
}

KoAbstractGradientSP KisTool::currentGradient()
{
    return d->currentGradient;
}

KisPaintOpPresetSP KisTool::currentPaintOpPreset()
{
    PkVariant v = canvas()->resourceManager()->resource(KoCanvasResource::CurrentPaintOpPreset);
    if (v.isNull()) {
        return 0;
    }
    else {
        return v.value<KisPaintOpPresetSP>();
    }
}

KisNodeSP KisTool::currentNode() const
{
    KisNodeSP node = canvas()->resourceManager()->resource(KoCanvasResource::CurrentKritaNode).value<KisNodeWSP>();
    return node;
}

KisNodeList KisTool::selectedNodes() const
{
    return canvas()->resourceManager()
        ->resource(KoCanvasResource::CurrentKritaSelectedNodes)
        .value<KisNodeList>();
}

KoColor KisTool::currentFgColor()
{
    return d->currentFgColor;
}

KoColor KisTool::currentBgColor()
{
    return d->currentBgColor;
}

KisImageWSP KisTool::currentImage()
{
    return image();
}

KisFilterConfigurationSP  KisTool::currentGenerator()
{
    return d->currentGenerator;
}

void KisTool::setMode(ToolMode mode) {
    d->m_mode = mode;
}

KisTool::ToolMode KisTool::mode() const {
    return d->m_mode;
}

void KisTool::setCursor(KisCanvasCursorToken cursor)
{
    d->cursor = cursor;
}

void KisTool::applyCursor(KisCanvasCursorToken cursor)
{
    KoToolBase::useCursor(cursor);
}

KisTool::AlternateAction KisTool::actionToAlternateAction(ToolAction action) {
    KIS_ASSERT_RECOVER_RETURN_VALUE(action != Primary, Secondary);
    return (AlternateAction)action;
}

void KisTool::activatePrimaryAction()
{
    resetCursorStyle();
}

void KisTool::deactivatePrimaryAction()
{
    resetCursorStyle();
}

void KisTool::beginPrimaryAction(KoPointerEvent *event)
{
    Q_UNUSED(event);
}

void KisTool::beginPrimaryDoubleClickAction(KoPointerEvent *event)
{
    beginPrimaryAction(event);
}

void KisTool::continuePrimaryAction(KoPointerEvent *event)
{
    Q_UNUSED(event);
}

void KisTool::endPrimaryAction(KoPointerEvent *event)
{
    Q_UNUSED(event);
}

bool KisTool::primaryActionSupportsHiResEvents() const
{
    return false;
}

void KisTool::activateAlternateAction(AlternateAction action)
{
    Q_UNUSED(action);
}

void KisTool::deactivateAlternateAction(AlternateAction action)
{
    Q_UNUSED(action);
}

void KisTool::beginAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    Q_UNUSED(event);
    Q_UNUSED(action);
}

void KisTool::beginAlternateDoubleClickAction(KoPointerEvent *event, AlternateAction action)
{
    beginAlternateAction(event, action);
}

void KisTool::continueAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    Q_UNUSED(event);
    Q_UNUSED(action);
}

void KisTool::endAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    Q_UNUSED(event);
    Q_UNUSED(action);
}

bool KisTool::alternateActionSupportsHiResEvents(AlternateAction action) const
{
    Q_UNUSED(action);
    return false;
}

bool KisTool::supportsPaintingAssistants() const
{
    return false;
}

void KisTool::mouseDoubleClickEvent(KoPointerEvent *event)
{
    Q_UNUSED(event);
}

void KisTool::mouseTripleClickEvent(KoPointerEvent *event)
{
    mouseDoubleClickEvent(event);
}

void KisTool::mousePressEvent(KoPointerEvent *event)
{
    Q_UNUSED(event);
}

void KisTool::mouseReleaseEvent(KoPointerEvent *event)
{
    Q_UNUSED(event);
}

void KisTool::mouseMoveEvent(KoPointerEvent *event)
{
    Q_UNUSED(event);
}

void KisTool::deleteSelection()
{
    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image(), currentNode(), this->canvas()->resourceManager()->canvasResourcesInterface(), 0, selectedNodes());

    if (!blockUntilOperationsFinished()) {
        return;
    }

    if (!clearImage(image(), resources->selectedNodes(), resources->activeSelection())) {
        KoToolBase::deleteSelection();
    }
}

KisTool::NodePaintAbility KisTool::nodePaintAbility()
{
    KisNodeSP node = currentNode();

    if (canvas()->resourceManager()->resource(KoCanvasResource::CurrentPaintOpPreset).isNull()) {
        return NodePaintAbility::UNPAINTABLE;
    }

    if (!node) {
        return NodePaintAbility::UNPAINTABLE;
    }

    if (node->inherits("KisShapeLayer")) {
        return NodePaintAbility::VECTOR;
    }
    if (node->inherits("KisCloneLayer")) {
        return NodePaintAbility::CLONE;
    }
    if (node->paintDevice()) {

        KisPaintOpPresetSP currentPaintOpPreset = canvas()->resourceManager()->resource(KoCanvasResource::CurrentPaintOpPreset).value<KisPaintOpPresetSP>();
        if (currentPaintOpPreset->paintOp().id() == "mypaintbrush") {
            const KoColorSpace *colorSpace = node->paintDevice()->colorSpace();
            if (colorSpace->colorModelId() != RGBAColorModelID) {
                return NodePaintAbility::MYPAINTBRUSH_UNPAINTABLE;
            }
        }

        return NodePaintAbility::PAINT;
    }

    return NodePaintAbility::UNPAINTABLE;
}

void KisTool::newActivationWithExternalSource(KisPaintDeviceSP externalSource)
{
    Q_UNUSED(externalSource);
}

#define NEAR_VAL -1000.0
#define FAR_VAL 1000.0
#define PROGRAM_VERTEX_ATTRIBUTE 0

void KisTool::paintToolOutline(PkPainter *painter, const KisOptimizedBrushOutline &path)
{
    if (KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas())) {
        services->drawToolOutline(painter, path, decorationThickness());
    }
}

void KisTool::resetCursorStyle()
{
    applyCursor(d->cursor);
}

bool KisTool::overrideCursorIfNotEditable()
{
    // override cursor for canvas iff this tool is active
    // and we can't paint on the active layer
    if (isActive()) {
        KisNodeSP node = currentNode();
        if (node && !node->isEditable()) {
            KisCanvasToolServices *services =
                dynamic_cast<KisCanvasToolServices *>(canvas());
            applyCursor(services->toolForbiddenCursorToken());
            return true;
        }
    }
    return false;
}

bool KisTool::blockUntilOperationsFinished()
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    return services && services->toolBlockUntilOperationsFinished(image());
}

void KisTool::blockUntilOperationsFinishedForced()
{
    if (KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas())) {
        services->toolBlockUntilOperationsFinishedForced(image());
    }
}

bool KisTool::isActive() const
{
    return d->m_isActive;
}

bool KisTool::nodeEditable()
{
    KisNodeSP node = currentNode();
    if (!node) {
        return false;
    }

    if (!currentPaintOpPreset()) {
        return false;
    }

    bool blockedNoIndirectPainting = false;

    const bool presetUsesIndirectPainting =
        !currentPaintOpPreset()->settings()->paintIncremental();

    if (!presetUsesIndirectPainting) {
        const KisIndirectPaintingSupport *indirectPaintingLayer =
                dynamic_cast<const KisIndirectPaintingSupport*>(node.data());
        if (indirectPaintingLayer) {
            blockedNoIndirectPainting = !indirectPaintingLayer->supportsNonIndirectPainting();
        }
    }

    bool nodeEditable = node->isEditable() && !blockedNoIndirectPainting;

    if (!nodeEditable) {
        if (KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas())) {
            services->toolShowFloatingMessage(
                nodeEditableMessage(node, blockedNoIndirectPainting));
        }
    }
    return nodeEditable;
}

bool KisTool::selectionEditable()
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    bool editable = services && services->toolSelectionEditable();
    if (!editable) {
        if (KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas())) {
            services->toolShowFloatingMessage(PkString("Local selection is locked."));
        }
    }
    return editable;
}

void KisTool::listenToModifiers(bool listen)
{
    Q_UNUSED(listen);
}

bool KisTool::listeningToModifiers()
{
    return false;
}
