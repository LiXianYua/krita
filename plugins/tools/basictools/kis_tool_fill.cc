/*
*  kis_tool_fill.cc - part of Krayon
*
*  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@compuzone.net>
*  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
*  SPDX-FileCopyrightText: 2004 Bart Coppens <kde@bartcoppens.be>
*
*  SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kis_tool_fill.h"

#include <kis_debug.h>
#include <klocalizedstring.h>

#include <ksharedconfig.h>

#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoPointerEvent.h>

#include <kis_layer.h>
#include <resources/KoPattern.h>
#include <kis_selection.h>

#include <KisCanvasFeedback.h>
#include <KisCanvasToolServices.h>

#include <processing/fill_processing_visitor.h>
#include <kis_command_utils.h>
#include <kis_layer_utils.h>
#include <krita_utils.h>
#include <kis_stroke_strategy_undo_command_based.h>
#include <commands_new/kis_processing_command.h>
#include <commands_new/kis_update_command.h>
#include <kis_fill_painter.h>
#include <kis_selection_filters.h>

#include <kis_dummies_facade.h>
#include <KoShapeControllerBase.h>
#include <kis_shape_controller.h>
#include <kis_image_animation_interface.h>

KisToolFill::KisToolFill(KoCanvasBase * canvas)
    : KisToolPaint(canvas, dynamic_cast<KisCanvasToolServices *>(canvas)->toolLoadCursor("tool_fill_cursor.png", 6, 6))
    , m_fillMask(nullptr)
    , m_referencePaintDevice(nullptr)
    , m_referenceNodeList(nullptr)
    , m_previousTime(0)
    , m_compressorFillUpdate(150, KisSignalCompressor::FIRST_ACTIVE)
    , m_dirtyRect(nullptr)
    , m_fillStrokeId(nullptr)
{
    setObjectName("tool_fill");
    connect(&m_compressorFillUpdate, SIGNAL(timeout()), SLOT(slotUpdateFill()));

    connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const QVariant &) {
                if (key == KoCanvasResource::CurrentEffectiveCompositeOp) {
                    resetCursorStyle();
                }
            });
}

KisToolFill::~KisToolFill()
{
}

void KisToolFill::resetCursorStyle()
{
    if (isEraser() && !m_useCustomBlendingOptions) {
        useCursor(dynamic_cast<KisCanvasToolServices *>(canvas())->toolLoadCursor("tool_fill_eraser_cursor.png", 6, 6));
    } else {
        KisToolPaint::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisToolFill::activate(const QSet<KoShape*> &shapes)
{
    KisToolPaint::activate(shapes);
    m_configGroup = KSharedConfig::openConfig()->group(toolId());
    loadConfiguration();
}

void KisToolFill::deactivate()
{
    m_referencePaintDevice = nullptr;
    m_referenceNodeList = nullptr;
    KisToolPaint::deactivate();
}

void KisToolFill::beginPrimaryAction(KoPointerEvent *event)
{
    // cannot use fill tool on non-painting layers.
    // this logic triggers with multiple layer types like vector layer, clone layer, file layer, group layer
    if (currentNode().isNull() || currentNode()->inherits("KisShapeLayer") || nodePaintAbility()!=NodePaintAbility::PAINT ) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER(feedback) {
            event->ignore();
            return;
        }
        feedback->showFloatingMessage(
            i18n("You cannot use this tool with the selected layer type"),
            QIcon(), 2000, KisCanvasFeedback::Priority::Medium, Qt::AlignCenter);
        event->ignore();
        return;
    }

    if (!nodeEditable()) {
        event->ignore();
        return;
    }

    m_fillStartWidgetPosition = event->pos();
    const QPoint lastImagePosition = convertToImagePixelCoordFloored(event);

    if (!currentNode() ||
        (!image()->wrapAroundModePermitted() &&
         !image()->bounds().contains(lastImagePosition))) {
        return;
    }
    
    // Switch the fill mode if shift or alt modifiers are pressed
    if (event->modifiers() == Qt::ShiftModifier) {
        if (m_fillMode == FillMode_FillSimilarRegions) {
            m_effectiveFillMode = FillMode_FillSelection;
        } else {
            m_effectiveFillMode = FillMode_FillSimilarRegions;
        }
    } else if (event->modifiers() == Qt::AltModifier) {
        if (m_fillMode == FillMode_FillContiguousRegion) {
            m_effectiveFillMode = FillMode_FillSelection;
        } else {
            m_effectiveFillMode = FillMode_FillContiguousRegion;
        }
    } else {
        m_effectiveFillMode = m_fillMode;
    }

    m_seedPoints.append(lastImagePosition);
    beginFilling(lastImagePosition);
    m_isFilling = true;

    slotUpdateFill();
}

void KisToolFill::continuePrimaryAction(KoPointerEvent *event)
{
    if (!m_isFilling || m_effectiveFillMode != FillMode_FillContiguousRegion ||
        m_continuousFillMode == ContinuousFillMode_DoNotUse) {
        return;
    }
    
    if (!m_isDragging) {
        const int dragDistanceSquared =
            pow2(event->pos().x() - m_fillStartWidgetPosition.x()) +
            pow2(event->pos().y() - m_fillStartWidgetPosition.y());

        if (dragDistanceSquared < minimumDragDistanceSquared) {
            return;
        }

        m_isDragging = true;
    }

    const QPoint newImagePosition = convertToImagePixelCoordFloored(event);
    m_seedPoints.append(newImagePosition);

    m_compressorFillUpdate.start();
}

void KisToolFill::endPrimaryAction(KoPointerEvent *)
{
    if (m_isFilling) {
        m_compressorFillUpdate.stop();
        endFilling();
    }

    m_isFilling = false;
    m_isDragging = false;
    m_seedPoints.clear();
}

void KisToolFill::beginAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (action == ChangeSize) {
        beginPrimaryAction(event);
        return;
    }
    KisToolPaint::beginAlternateAction(event, action);
}

void KisToolFill::continueAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (action == ChangeSize) {
        continuePrimaryAction(event);
        return;
    }
    KisToolPaint::continueAlternateAction(event, action);
}

void KisToolFill::endAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (action == ChangeSize) {
        endPrimaryAction(event);
        return;
    }
    KisToolPaint::endAlternateAction(event, action);
}

void KisToolFill::beginFilling(const QPoint &seedPoint)
{
    setMode(KisTool::PAINT_MODE);

    KisStrokeStrategyUndoCommandBased *strategy =
            new KisStrokeStrategyUndoCommandBased(kundo2_i18n("Flood Fill"), false, image().data());
    strategy->setSupportsWrapAroundMode(true);
    m_fillStrokeId = image()->startStroke(strategy);
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_fillStrokeId);

    m_resourcesSnapshot = new KisResourcesSnapshot(image(), currentNode(), this->canvas()->resourceManager()->canvasResourcesInterface());

    KisPaintDeviceSP referencePaintDevice = nullptr;
    if (m_effectiveFillMode != FillMode_FillSelection) {
        if (m_reference == Reference_CurrentLayer) {
            referencePaintDevice = currentNode()->paintDevice();
        } else if (m_reference == Reference_AllLayers) {
            referencePaintDevice = currentImage()->projection();
        } else if (m_reference == Reference_ColorLabeledLayers) {
            if (!m_referenceNodeList) {
                referencePaintDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(image(), "Fill Tool Reference Result Paint Device");
                m_referenceNodeList.reset(new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
            } else {
                referencePaintDevice = m_referencePaintDevice;
            }
            KisPaintDeviceSP newReferencePaintDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(image(), "Fill Tool Reference Result Paint Device");
            KisMergeLabeledLayersCommand::ReferenceNodeInfoListSP newReferenceNodeList(new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
            const int currentTime = image()->animationInterface()->currentTime();
            image()->addJob(
                m_fillStrokeId,
                new KisStrokeStrategyUndoCommandBased::Data(
                    KUndo2CommandSP(new KisMergeLabeledLayersCommand(
                        image(),
                        m_referenceNodeList,
                        newReferenceNodeList,
                        referencePaintDevice,
                        newReferencePaintDevice,
                        m_selectedColorLabels,
                        KisMergeLabeledLayersCommand::GroupSelectionPolicy_SelectIfColorLabeled,
                        m_previousTime != currentTime,
                        m_useActiveLayer ? currentNode() : nullptr
                    )),
                    false,
                    KisStrokeJobData::SEQUENTIAL,
                    KisStrokeJobData::EXCLUSIVE
                )
            );
            referencePaintDevice = newReferencePaintDevice;
            m_referenceNodeList = newReferenceNodeList;
            m_previousTime = currentTime;
        }

        QSharedPointer<KoColor> referenceColor(new KoColor);
        if (m_reference == Reference_ColorLabeledLayers) {
            // We need to obtain the reference color from the reference paint
            // device, but it is produced in a stroke, so we must get the color
            // after the device is ready. So we get it in the stroke
            image()->addJob(
                m_fillStrokeId,
                new KisStrokeStrategyUndoCommandBased::Data(
                    KUndo2CommandSP(new KisCommandUtils::LambdaCommand(
                        [referencePaintDevice, referenceColor, seedPoint]() -> KUndo2Command*
                        {
                            *referenceColor = referencePaintDevice->pixel(seedPoint);
                            return 0;
                        }
                    )),
                    false,
                    KisStrokeJobData::SEQUENTIAL,
                    KisStrokeJobData::EXCLUSIVE
                )
            );
        } else {
            // Here the reference device is already ready, so we obtain the
            // reference color directly
            *referenceColor = referencePaintDevice->pixel(seedPoint);
            // Reset this so that the device from color labeled layers gets
            // regenerated when that mode is selected again
            m_referenceNodeList.reset();
        }

        m_referencePaintDevice = referencePaintDevice;
        m_referenceColor = referenceColor;

        m_fillMask = new KisSelection;
    }

    m_dirtyRect.reset(new QRect);
    m_transform.reset();
    m_transform.rotate(m_patternRotation);
    const qreal normalizedScale = m_patternScale * 0.01;
    m_transform.scale(normalizedScale, normalizedScale);
    m_resourcesSnapshot->setFillTransform(m_transform);
}

void KisToolFill::addFillingOperation(const QPoint &seedPoint)
{
    const QVector<QPoint> seedPoints({seedPoint});
    addFillingOperation(seedPoints);
}

void KisToolFill::addFillingOperation(const QVector<QPoint> &seedPoints)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_fillStrokeId);

    const qreal customOpacity = m_customOpacity / 100.0;

    if (m_effectiveFillMode != FillMode_FillSimilarRegions) {
        FillProcessingVisitor *visitor =  new FillProcessingVisitor(m_referencePaintDevice,
                                                                    m_resourcesSnapshot->activeSelection(),
                                                                    m_resourcesSnapshot);

        const bool blendingOptionsAreNoOp = m_useCustomBlendingOptions
                                            ? (qFuzzyCompare(customOpacity, OPACITY_OPAQUE_F) &&
                                               m_customCompositeOp == COMPOSITE_OVER)
                                            : (qFuzzyCompare(m_resourcesSnapshot->opacity(), OPACITY_OPAQUE_F) &&
                                               m_resourcesSnapshot->compositeOpId() == COMPOSITE_OVER);

        const bool useFastMode = !m_resourcesSnapshot->activeSelection() &&
                                 blendingOptionsAreNoOp &&
                                 m_fillType != FillType_FillWithPattern &&
                                 m_opacitySpread == 100 &&
                                 m_useSelectionAsBoundary == false &&
                                 !m_antiAlias && m_sizemod == 0 && m_feather == 0 &&
                                 m_closeGap == 0 &&
                                 m_reference == Reference_CurrentLayer;

        visitor->setSeedPoints(seedPoints);
        visitor->setUseFastMode(useFastMode);
        visitor->setSelectionOnly(m_effectiveFillMode == FillMode_FillSelection);
        visitor->setUseBgColor(m_fillType == FillType_FillWithBackgroundColor);
        visitor->setUsePattern(m_fillType == FillType_FillWithPattern);
        visitor->setUseCustomBlendingOptions(m_useCustomBlendingOptions);
        if (m_useCustomBlendingOptions) {
            visitor->setCustomOpacity(customOpacity);
            visitor->setCustomCompositeOp(m_customCompositeOp);
        }
        visitor->setRegionFillingMode(
            m_contiguousFillMode == ContiguousFillMode_FloodFill
            ? KisFillPainter::RegionFillingMode_FloodFill
            : KisFillPainter::RegionFillingMode_BoundaryFill
        );
        if (m_contiguousFillMode == ContiguousFillMode_BoundaryFill) {
            visitor->setRegionFillingBoundaryColor(m_contiguousFillBoundaryColor);
        }
        visitor->setFillThreshold(m_threshold);
        visitor->setOpacitySpread(m_opacitySpread);
        visitor->setCloseGap(m_closeGap);
        visitor->setUseSelectionAsBoundary(m_useSelectionAsBoundary);
        visitor->setAntiAlias(m_antiAlias);
        visitor->setSizeMod(m_sizemod);
        visitor->setStopGrowingAtDarkestPixel(m_stopGrowingAtDarkestPixel);
        visitor->setFeather(m_feather);
        if (m_isDragging) {
            visitor->setContinuousFillMode(
                m_continuousFillMode == ContinuousFillMode_FillAnyRegion
                ? FillProcessingVisitor::ContinuousFillMode_FillAnyRegion
                : FillProcessingVisitor::ContinuousFillMode_FillSimilarRegions
            );
            visitor->setContinuousFillMask(m_fillMask);
            visitor->setContinuousFillReferenceColor(m_referenceColor);
        }
        visitor->setOutDirtyRect(m_dirtyRect);

        image()->addJob(
            m_fillStrokeId,
            new KisStrokeStrategyUndoCommandBased::Data(
                KUndo2CommandSP(new KisProcessingCommand(visitor, currentNode())),
                false,
                KisStrokeJobData::SEQUENTIAL,
                KisStrokeJobData::EXCLUSIVE
            )
        );
    } else {
        KisSelectionSP fillMask = m_fillMask;
        QSharedPointer<KisProcessingVisitor::ProgressHelper>
            progressHelper(new KisProcessingVisitor::ProgressHelper(currentNode()));

        {
            KisSelectionSP selection = m_resourcesSnapshot->activeSelection();
            KisFillPainter painter;
            QRect bounds = currentImage()->bounds();
            if (selection) {
                bounds = bounds.intersected(selection->projection()->selectedRect());
            }

            painter.setFillThreshold(m_threshold);
            painter.setOpacitySpread(m_opacitySpread);
            painter.setAntiAlias(m_antiAlias);
            painter.setSizemod(m_sizemod);
            painter.setStopGrowingAtDarkestPixel(m_stopGrowingAtDarkestPixel);
            painter.setFeather(m_feather);

            QVector<KisStrokeJobData*> jobs =
                painter.createSimilarColorsSelectionJobs(
                    fillMask->pixelSelection(), m_referenceColor, m_referencePaintDevice,
                    bounds, selection ? selection->projection() : nullptr, progressHelper
                );

            for (KisStrokeJobData *job : jobs) {
                image()->addJob(m_fillStrokeId, job);
            }
        }

        {
            FillProcessingVisitor *visitor =  new FillProcessingVisitor(nullptr,
                                                                        fillMask,
                                                                        m_resourcesSnapshot);

            visitor->setSeedPoints(seedPoints);
            visitor->setSelectionOnly(true);
            visitor->setUseBgColor(m_fillType == FillType_FillWithBackgroundColor);
            visitor->setUsePattern(m_fillType == FillType_FillWithPattern);
            visitor->setUseCustomBlendingOptions(m_useCustomBlendingOptions);
            if (m_useCustomBlendingOptions) {
                visitor->setCustomOpacity(customOpacity);
                visitor->setCustomCompositeOp(m_customCompositeOp);
            }
            visitor->setOutDirtyRect(m_dirtyRect);
            visitor->setProgressHelper(progressHelper);

            image()->addJob(
                m_fillStrokeId,
                new KisStrokeStrategyUndoCommandBased::Data(
                    KUndo2CommandSP(new KisProcessingCommand(visitor, currentNode())),
                    false,
                    KisStrokeJobData::SEQUENTIAL,
                    KisStrokeJobData::EXCLUSIVE
                )
            );
        }
    }
}

void KisToolFill::addUpdateOperation()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_fillStrokeId);

    image()->addJob(
        m_fillStrokeId,
        new KisStrokeStrategyUndoCommandBased::Data(
            KUndo2CommandSP(new KisUpdateCommand(currentNode(), m_dirtyRect, image().data())),
            false,
            KisStrokeJobData::SEQUENTIAL,
            KisStrokeJobData::EXCLUSIVE
        )
    );
}

void KisToolFill::endFilling()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_fillStrokeId);
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    setMode(KisTool::HOVER_MODE);
    image()->endStroke(m_fillStrokeId);
    m_fillStrokeId = nullptr;
    m_fillMask = nullptr;
    m_dirtyRect = nullptr;
}

void KisToolFill::slotUpdateFill()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_fillStrokeId);

    if (m_effectiveFillMode == FillMode_FillContiguousRegion) {
        addFillingOperation(KritaUtils::rasterizePolylineDDA(m_seedPoints));
        // clear to not re-add the segments, but retain the last point to maintain continuity
        m_seedPoints = {m_seedPoints.last()};
    } else {
        addFillingOperation(m_seedPoints.last());
    }
    addUpdateOperation();
}

void KisToolFill::loadConfiguration()
{
    {
        const QString whatToFillStr = m_configGroup.readEntry<QString>("whatToFill", "");
        if (whatToFillStr == "fillSelection") {
            m_fillMode = FillMode_FillSelection;
        } else if (whatToFillStr == "fillContiguousRegion") {
            m_fillMode = FillMode_FillContiguousRegion;
        } else if (whatToFillStr == "fillSimilarRegions") {
            m_fillMode = FillMode_FillSimilarRegions;
        } else {
            if (m_configGroup.readEntry<bool>("fillSelection", false)) {
                m_fillMode = FillMode_FillSelection;
            } else {
                m_fillMode = FillMode_FillContiguousRegion;
            }
        }
    }
    {
        const QString fillTypeStr = m_configGroup.readEntry<QString>("fillWith", "");
        if (fillTypeStr == "foregroundColor") {
            m_fillType = FillType_FillWithForegroundColor;
        } else if (fillTypeStr == "backgroundColor") {
            m_fillType = FillType_FillWithBackgroundColor;
        } else if (fillTypeStr == "pattern") {
            m_fillType = FillType_FillWithPattern;
        } else {
            if (m_configGroup.readEntry<bool>("usePattern", false)) {
                m_fillType = FillType_FillWithPattern;
            } else {
                m_fillType = FillType_FillWithForegroundColor;
            }
        }
    }
    m_patternScale = m_configGroup.readEntry<qreal>("patternScale", 100.0);
    m_patternRotation = m_configGroup.readEntry<qreal>("patternRotate", 0.0);
    m_useCustomBlendingOptions = m_configGroup.readEntry<bool>("useCustomBlendingOptions", false);
    m_customOpacity = qBound(0, m_configGroup.readEntry<int>("customOpacity", 100), 100);
    m_customCompositeOp = m_configGroup.readEntry<QString>("customCompositeOp", COMPOSITE_OVER);
    if (KoCompositeOpRegistry::instance().getKoID(m_customCompositeOp).id().isNull()) {
        m_customCompositeOp = COMPOSITE_OVER;
    }
    {
        const QString contiguousFillModeStr = m_configGroup.readEntry<QString>("contiguousFillMode", "");
        m_contiguousFillMode = contiguousFillModeStr == "boundaryFill"
                               ? ContiguousFillMode_BoundaryFill
                               : ContiguousFillMode_FloodFill;
    }
    m_contiguousFillBoundaryColor = loadContiguousFillBoundaryColorFromConfig();
    m_threshold = m_configGroup.readEntry<int>("thresholdAmount", 8);
    m_opacitySpread = m_configGroup.readEntry<int>("opacitySpread", 100);
    m_closeGap = m_configGroup.readEntry<int>("closeGapAmount", 0);
    m_useSelectionAsBoundary = m_configGroup.readEntry<bool>("useSelectionAsBoundary", true);
    m_antiAlias = m_configGroup.readEntry<bool>("antiAlias", false);
    m_sizemod = m_configGroup.readEntry<int>("growSelection", 0);
    m_stopGrowingAtDarkestPixel = m_configGroup.readEntry<bool>("stopGrowingAtDarkestPixel", false);
    m_feather = m_configGroup.readEntry<int>("featherAmount", 0);
    {
        const QString sampleLayersModeStr = m_configGroup.readEntry<QString>("sampleLayersMode", "");
        if (sampleLayersModeStr == "currentLayer") {
            m_reference = Reference_CurrentLayer;
        } else if (sampleLayersModeStr == "allLayers") {
            m_reference = Reference_AllLayers;
        } else if (sampleLayersModeStr == "colorLabeledLayers") {
            m_reference = Reference_ColorLabeledLayers;
        } else {
            if (m_configGroup.readEntry<bool>("sampleMerged", false)) {
                m_reference = Reference_AllLayers;
            } else {
                m_reference = Reference_CurrentLayer;
            }
        }
    }
    {
        const QStringList colorLabelsStr = m_configGroup.readEntry<QString>("colorLabels", "").split(',', Qt::SkipEmptyParts);
        m_selectedColorLabels.clear();
        for (const QString &colorLabelStr : colorLabelsStr) {
            bool ok;
            const int colorLabel = colorLabelStr.toInt(&ok);
            if (ok) {
                m_selectedColorLabels << colorLabel;
            }
        }
        m_useActiveLayer = m_configGroup.readEntry<bool>("useActiveLayer", false);
    }
    {
        const QString continuousFillModeStr = m_configGroup.readEntry<QString>("continuousFillMode", "fillAnyRegion");
        if (continuousFillModeStr == "doNotUse") {
            m_continuousFillMode = ContinuousFillMode_DoNotUse;
        } else if (continuousFillModeStr == "fillSimilarRegions") {
            m_continuousFillMode = ContinuousFillMode_FillSimilarRegions;
        } else {
            m_continuousFillMode = ContinuousFillMode_FillAnyRegion;
        }
    }
}

KoColor KisToolFill::loadContiguousFillBoundaryColorFromConfig()
{
    const QString xmlColor = m_configGroup.readEntry("contiguousFillBoundaryColor", QString());
    QDomDocument doc;
    if (doc.setContent(xmlColor)) {
        QDomElement e = doc.documentElement().firstChild().toElement();
        QString channelDepthID = doc.documentElement().attribute("channeldepth", Integer16BitsColorDepthID.id());
        bool ok;
        if (e.hasAttribute("space") || e.tagName().toLower() == "srgb") {
            return KoColor::fromXML(e, channelDepthID, &ok);
        } else if (doc.documentElement().hasAttribute("space") || doc.documentElement().tagName().toLower() == "srgb"){
            return KoColor::fromXML(doc.documentElement(), channelDepthID, &ok);
        }
    }
    return KoColor();
}
