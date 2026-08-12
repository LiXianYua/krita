/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <kis_debug.h>
#include <klocalizedstring.h>

#include <KisOptionCollectionWidget.h>
#include <KoGroupButton.h>

#include <ksharedconfig.h>

#include <KoCanvasBase.h>
#include <KoPointerEvent.h>

#include <kis_layer.h>
#include <kis_painter.h>
#include <resources/KoPattern.h>
#include <kis_selection.h>

#include <KisViewManager.h>
#include <canvas/kis_canvas2.h>
#include <kis_cursor.h>
#include "kis_resources_snapshot.h"
#include <kis_color_button.h>
#include <kis_color_label_selector_widget.h>
#include <kis_cmb_composite.h>
#include <kis_image_animation_interface.h>

#include <kis_stroke_strategy_undo_command_based.h>
#include <commands_new/kis_processing_command.h>
#include <commands_new/kis_update_command.h>
#include <kis_command_utils.h>
#include <functional>
#include <kis_group_layer.h>
#include <kis_layer_utils.h>

#include <KisPart.h>
#include <KisDocument.h>
#include <kis_dummies_facade.h>
#include <KoShapeControllerBase.h>
#include <kis_shape_controller.h>
#include <kis_canvas_resource_provider.h>

#include <KoCompositeOpRegistry.h>

#include <processing/KisEncloseAndFillProcessingVisitor.h>

#include "KisToolEncloseAndFill.h"
#include "subtools/KisRectangleEnclosingProducer.h"
#include "subtools/KisEllipseEnclosingProducer.h"
#include "subtools/KisPathEnclosingProducer.h"
#include "subtools/KisLassoEnclosingProducer.h"
#include "subtools/KisBrushEnclosingProducer.h"

KisToolEncloseAndFill::KisToolEncloseAndFill(KoCanvasBase * canvas)
    : KisDynamicDelegatedTool<KisToolShape>(canvas, QCursor())
{
    setObjectName("tool_enclose_and_fill");
}

KisToolEncloseAndFill::~KisToolEncloseAndFill()
{}

void KisToolEncloseAndFill::resetCursorStyle()
{
    KisDynamicDelegatedTool::resetCursorStyle();
    overrideCursorIfNotEditable();
}

void KisToolEncloseAndFill::activate(const QSet<KoShape*> &shapes)
{
    KisDynamicDelegatedTool::activate(shapes);
    m_configGroup = KSharedConfig::openConfig()->group(toolId());

    // Was only called from createOptionWidget() (now deleted), which ran on
    // every tool activation, so this keeps the same effective defaults
    // (config-driven, via the same m_configGroup keys) without a panel.
    loadConfiguration();

    KisCanvas2 *kisCanvas = static_cast<KisCanvas2*>(canvas());
    KisCanvasResourceProvider *resourceProvider = kisCanvas->viewManager()->canvasResourceProvider();
    if (resourceProvider) {
        connect(resourceProvider,
                SIGNAL(sigNodeChanged(const KisNodeSP)),
                this,
                SLOT(slot_currentNodeChanged(const KisNodeSP)));
        slot_currentNodeChanged(currentNode());
    }
}

void KisToolEncloseAndFill::deactivate()
{
    m_referencePaintDevice = nullptr;
    m_referenceNodeList = nullptr;
    KisCanvas2 *kisCanvas = static_cast<KisCanvas2*>(canvas());
    KisCanvasResourceProvider *resourceProvider = kisCanvas->viewManager()->canvasResourceProvider();
    if (resourceProvider) {
        disconnect(resourceProvider,
                   SIGNAL(sigNodeChanged(const KisNodeSP)),
                   this,
                   SLOT(slot_currentNodeChanged(const KisNodeSP)));
    }
    slot_currentNodeChanged(nullptr);
    KisDynamicDelegatedTool::deactivate();
}

void KisToolEncloseAndFill::setupEnclosingSubtool()
{
    if (delegateTool()) {
        delegateTool()->deactivate();
    }

    if (m_enclosingMethod == Ellipse) {
        KisEllipseEnclosingProducer *newDelegateTool = new KisEllipseEnclosingProducer(canvas());
        setDelegateTool(reinterpret_cast<KisDynamicDelegateTool<KisToolShape>*>(newDelegateTool));
        setCursor(newDelegateTool->cursor());
    } else if (m_enclosingMethod == Path) {
        KisPathEnclosingProducer *newDelegateTool = new KisPathEnclosingProducer(canvas());
        setDelegateTool(reinterpret_cast<KisDynamicDelegateTool<KisToolShape>*>(newDelegateTool));
        setCursor(newDelegateTool->cursor());
    } else if (m_enclosingMethod == Lasso) {
        KisLassoEnclosingProducer *newDelegateTool = new KisLassoEnclosingProducer(canvas());
        setDelegateTool(reinterpret_cast<KisDynamicDelegateTool<KisToolShape>*>(newDelegateTool));
        setCursor(newDelegateTool->cursor());
    } else if (m_enclosingMethod == Brush) {
        KisBrushEnclosingProducer *newDelegateTool = new KisBrushEnclosingProducer(canvas());
        setDelegateTool(reinterpret_cast<KisDynamicDelegateTool<KisToolShape>*>(newDelegateTool));
        setCursor(newDelegateTool->cursor());
    } else {
        KisRectangleEnclosingProducer *newDelegateTool = new KisRectangleEnclosingProducer(canvas());
        setDelegateTool(reinterpret_cast<KisDynamicDelegateTool<KisToolShape>*>(newDelegateTool));
        setCursor(newDelegateTool->cursor());
    }

    connect(delegateTool(), SIGNAL(enclosingMaskProduced(KisPixelSelectionSP)), SLOT(slot_delegateTool_enclosingMaskProduced(KisPixelSelectionSP)));

    if (isActivated()) {
        delegateTool()->activate(QSet<KoShape*>());
    }
}

bool KisToolEncloseAndFill::subtoolHasUserInteractionRunning() const
{
    if (!delegateTool()) {
        return false;
    }
    
    if (m_enclosingMethod == Rectangle) {
        return reinterpret_cast<KisRectangleEnclosingProducer*>(delegateTool())->hasUserInteractionRunning();
    } else if (m_enclosingMethod == Ellipse) {
        return reinterpret_cast<KisEllipseEnclosingProducer*>(delegateTool())->hasUserInteractionRunning();
    } else if (m_enclosingMethod == Path) {
        return reinterpret_cast<KisPathEnclosingProducer*>(delegateTool())->hasUserInteractionRunning();
    } else if (m_enclosingMethod == Lasso) {
        return reinterpret_cast<KisLassoEnclosingProducer*>(delegateTool())->hasUserInteractionRunning();
    } else if (m_enclosingMethod == Brush) {
        return reinterpret_cast<KisBrushEnclosingProducer*>(delegateTool())->hasUserInteractionRunning();
    }
    return false;
}

void KisToolEncloseAndFill::beginPrimaryAction(KoPointerEvent *event)
{
    // cannot use enclose and fill tool on non-painting layers.
    // this logic triggers with multiple layer types like vector layer, clone layer, file layer, group layer
    if (currentNode().isNull() || currentNode()->inherits("KisShapeLayer") || nodePaintAbility() != NodePaintAbility::PAINT) {
        KisCanvas2 * kiscanvas = static_cast<KisCanvas2*>(canvas());
        kiscanvas->viewManager()->
                showFloatingMessage(
                    i18n("You cannot use this tool with the selected layer type"),
                    QIcon(), 2000, KisFloatingMessage::Medium, Qt::AlignCenter);
        event->ignore();
        return;
    }

    if (!nodeEditable()) {
        event->ignore();
        return;
    }

    KisDynamicDelegatedTool::beginPrimaryAction(event);
}

void KisToolEncloseAndFill::activateAlternateAction(AlternateAction action)
{
    if (subtoolHasUserInteractionRunning()) {
        if (delegateTool()) {
            delegateTool()->activatePrimaryAction();
        }
        return;
    }
    KisDynamicDelegatedTool::activateAlternateAction(action);
}

void KisToolEncloseAndFill::deactivateAlternateAction(AlternateAction action)
{
    if (subtoolHasUserInteractionRunning()) {
        return;
    }
    KisDynamicDelegatedTool::deactivateAlternateAction(action);
}

void KisToolEncloseAndFill::beginAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (subtoolHasUserInteractionRunning()) {
        if (delegateTool()) {
            delegateTool()->beginPrimaryAction(event);
        }
        return;
    }
    KisDynamicDelegatedTool::beginAlternateAction(event, action);
    m_alternateActionStarted = true;
}

void KisToolEncloseAndFill::continueAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (subtoolHasUserInteractionRunning()) {
        if (delegateTool()) {
            delegateTool()->continuePrimaryAction(event);
        }
        return;
    }
    if (!m_alternateActionStarted) {
        return;
    }
    KisDynamicDelegatedTool::continueAlternateAction(event, action);
}

void KisToolEncloseAndFill::endAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (subtoolHasUserInteractionRunning()) {
        if (delegateTool()) {
            delegateTool()->endPrimaryAction(event);
        }
        return;
    }
    if (!m_alternateActionStarted) {
        return;
    }
    KisDynamicDelegatedTool::endAlternateAction(event, action);
    m_alternateActionStarted = false;
}

void KisToolEncloseAndFill::beginAlternateDoubleClickAction(KoPointerEvent *event, AlternateAction action)
{
    if (subtoolHasUserInteractionRunning()) {
        if (delegateTool()) {
            delegateTool()->beginPrimaryDoubleClickAction(event);
        }
        return;
    }
    KisDynamicDelegatedTool::beginAlternateDoubleClickAction(event, action);
}

void KisToolEncloseAndFill::slot_delegateTool_enclosingMaskProduced(KisPixelSelectionSP enclosingMask)
{
    KisStrokeStrategyUndoCommandBased *strategy =
            new KisStrokeStrategyUndoCommandBased(kundo2_i18n("Enclose and Fill"), false, image().data());
    strategy->setSupportsWrapAroundMode(true);
    m_fillStrokeId = image()->startStroke(strategy);
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_fillStrokeId);

    m_dirtyRect.reset(new QRect);

    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image(), currentNode(), this->canvas()->resourceManager());

    if (m_reference == CurrentLayer) {
        m_referencePaintDevice = currentNode()->paintDevice();
    } else if (m_reference == AllLayers) {
        m_referencePaintDevice = currentImage()->projection();
    } else if (m_reference == ColorLabeledLayers) {
        if (!m_referenceNodeList) {
            m_referencePaintDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(image(), "Enclose and Fill Tool Reference Result Paint Device");
            m_referenceNodeList.reset(new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
        }
        KisPaintDeviceSP newReferencePaintDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(image(), "Enclose and Fill Tool Reference Result Paint Device");
        KisMergeLabeledLayersCommand::ReferenceNodeInfoListSP newReferenceNodeList(new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
        const int currentTime = image()->animationInterface()->currentTime();
        image()->addJob(
            m_fillStrokeId,
            new KisStrokeStrategyUndoCommandBased::Data(
                KUndo2CommandSP(new KisMergeLabeledLayersCommand(
                    image(),
                    m_referenceNodeList,
                    newReferenceNodeList,
                    m_referencePaintDevice,
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
        m_referencePaintDevice = newReferencePaintDevice;
        m_referenceNodeList = newReferenceNodeList;
        m_previousTime = currentTime;
    }

    if (m_reference != ColorLabeledLayers) {
        // Reset this so that the device from color labeled layers gets
        // regenerated when that mode is selected again
        m_referenceNodeList.reset();
    }

    QTransform transform;
    transform.rotate(m_patternRotation);
    const qreal normalizedScale = m_patternScale * 0.01;
    transform.scale(normalizedScale, normalizedScale);
    resources->setFillTransform(transform);

    KisProcessingVisitorSP visitor =
        new KisEncloseAndFillProcessingVisitor(m_referencePaintDevice,
                                               enclosingMask,
                                               resources->activeSelection(),
                                               resources,
                                               m_regionSelectionMethod,
                                               m_regionSelectionColor,
                                               m_regionSelectionInvert,
                                               m_regionSelectionIncludeContourRegions,
                                               false,
                                               m_fillThreshold,
                                               m_fillOpacitySpread,
                                               m_closeGap,
                                               m_antiAlias,
                                               m_expand,
                                               m_stopGrowingAtDarkestPixel,
                                               m_feather,
                                               m_useSelectionAsBoundary,
                                               m_fillType == FillWithPattern,
                                               false,
                                               m_fillType == FillWithBackgroundColor,
                                               m_useCustomBlendingOptions,
                                               m_customOpacity / 100.0,
                                               m_customCompositeOp,
                                               m_dirtyRect);

    image()->addJob(
        m_fillStrokeId,
        new KisStrokeStrategyUndoCommandBased::Data(
            KUndo2CommandSP(new KisProcessingCommand(visitor, currentNode())),
            false,
            KisStrokeJobData::SEQUENTIAL,
            KisStrokeJobData::EXCLUSIVE
        )
    );

    image()->addJob(
        m_fillStrokeId,
        new KisStrokeStrategyUndoCommandBased::Data(
            KUndo2CommandSP(new KisUpdateCommand(currentNode(), m_dirtyRect, image().data())),
            false,
            KisStrokeJobData::SEQUENTIAL,
            KisStrokeJobData::EXCLUSIVE
        )
    );

    image()->endStroke(m_fillStrokeId);

    m_fillStrokeId = nullptr;
    m_dirtyRect = nullptr;
}

int KisToolEncloseAndFill::flags() const
{
    return KisDynamicDelegatedTool::flags() | KisTool::FLAG_USES_CUSTOM_SIZE | KisTool::FLAG_USES_CUSTOM_PRESET;
}

void KisToolEncloseAndFill::loadConfiguration()
{
    m_enclosingMethod = loadEnclosingMethodFromConfig();
    m_regionSelectionMethod = loadRegionSelectionMethodFromConfig();
    m_regionSelectionColor = loadRegionSelectionColorFromConfig();
    m_regionSelectionInvert = m_configGroup.readEntry<bool>("regionSelectionInvert", false);
    m_regionSelectionIncludeContourRegions = m_configGroup.readEntry<bool>("regionSelectionIncludeContourRegions", false);
    {
        const QString fillTypeStr = m_configGroup.readEntry<QString>("fillWith", "");
        if (fillTypeStr == "foregroundColor") {
            m_fillType = FillWithForegroundColor;
        } else if (fillTypeStr == "backgroundColor") {
            m_fillType = FillWithBackgroundColor;
        } else if (fillTypeStr == "pattern") {
            m_fillType = FillWithPattern;
        } else {
            if (m_configGroup.readEntry<bool>("usePattern", false)) {
                m_fillType = FillWithPattern;
            } else {
                m_fillType = FillWithForegroundColor;
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
    m_fillThreshold = m_configGroup.readEntry<int>("fillThreshold", 8);
    m_fillOpacitySpread = m_configGroup.readEntry<int>("fillOpacitySpread", 100);
    m_closeGap = m_configGroup.readEntry<int>("closeGapAmount", 0);
    m_useSelectionAsBoundary = m_configGroup.readEntry<bool>("useSelectionAsBoundary", true);
    m_antiAlias = m_configGroup.readEntry<bool>("antiAlias", false);
    m_expand = m_configGroup.readEntry<int>("expand", 0);
    m_stopGrowingAtDarkestPixel = m_configGroup.readEntry<bool>("stopGrowingAtDarkestPixel", false);
    m_feather = m_configGroup.readEntry<int>("feather", 0);
    {
        const QString sampleLayersModeStr = m_configGroup.readEntry<QString>("reference", "currentLayer");
        if (sampleLayersModeStr == "allLayers") {
            m_reference = AllLayers;
        } else if (sampleLayersModeStr == "colorLabeledLayers") {
            m_reference = ColorLabeledLayers;
        } else {
            m_reference = CurrentLayer;
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
    }
    m_useActiveLayer = m_configGroup.readEntry<bool>("useActiveLayer", false);

    setupEnclosingSubtool();
}

KisToolEncloseAndFill::EnclosingMethod KisToolEncloseAndFill::loadEnclosingMethodFromConfig() const
{
    return configStringToEnclosingMethod(m_configGroup.readEntry("enclosingMethod", enclosingMethodToConfigString(defaultEnclosingMethod())));
}

void KisToolEncloseAndFill::saveEnclosingMethodToConfig(EnclosingMethod enclosingMethod)
{
    m_configGroup.writeEntry("enclosingMethod", enclosingMethodToConfigString(enclosingMethod));
}

QString KisToolEncloseAndFill::enclosingMethodToConfigString(EnclosingMethod enclosingMethod) const
{
    switch (enclosingMethod) {
        case Rectangle: return "rectangle";
        case Ellipse: return "ellipse";
        case Path: return "path";
        case Brush: return "brush";
        default: return "lasso";
    }
}

KisToolEncloseAndFill::EnclosingMethod KisToolEncloseAndFill::configStringToEnclosingMethod(const QString &configString) const
{
    if (configString == "rectangle") {
        return Rectangle;
    } else if (configString == "ellipse") {
        return Ellipse;
    } else if (configString == "path") {
        return Path;
    } else if (configString == "brush") {
        return Brush;
    }
    return Lasso;
}

QString KisToolEncloseAndFill::regionSelectionMethodToUserString(RegionSelectionMethod regionSelectionMethod) const
{
    if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegions) {
        return i18nc("Region selection method in enclose and fill tool",
                     "All");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColor) {
        return i18nc("Region selection method in enclose and fill tool",
                     "Specific color");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithTransparent) {
        return i18nc("Region selection method in enclose and fill tool",
                     "Transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColorOrTransparent) {
        return i18nc("Region selection method in enclose and fill tool",
                     "Specific color or transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColor) {
        return i18nc("Region selection method in enclose and fill tool",
                     "All, excluding a specific color");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithTransparent) {
        return i18nc("Region selection method in enclose and fill tool",
                     "All, excluding transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColorOrTransparent) {
        return i18nc("Region selection method in enclose and fill tool",
                     "All, excluding a specific color or transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedBySpecificColor) {
        return i18nc("Region selection method in enclose and fill tool",
                     "Any surrounded by a specific color");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedByTransparent) {
        return i18nc("Region selection method in enclose and fill tool",
                     "Any surrounded by transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedBySpecificColorOrTransparent) {
        return i18nc("Region selection method in enclose and fill tool",
                     "Any surrounded by a specific color or transparency");
    }
    return QString();
}

KisToolEncloseAndFill::RegionSelectionMethod KisToolEncloseAndFill::loadRegionSelectionMethodFromConfig() const
{
    return configStringToRegionSelectionMethod(m_configGroup.readEntry("regionSelectionMethod", regionSelectionMethodToConfigString(defaultRegionSelectionMethod())));
}

void KisToolEncloseAndFill::saveRegionSelectionMethodToConfig(RegionSelectionMethod regionSelectionMethod)
{
    m_configGroup.writeEntry("regionSelectionMethod", regionSelectionMethodToConfigString(regionSelectionMethod));
}

QString KisToolEncloseAndFill::regionSelectionMethodToConfigString(RegionSelectionMethod regionSelectionMethod) const
{
    if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegions) {
        return "allRegions";
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColor) {
        return "regionsFilledWithSpecificColor";
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithTransparent) {
        return "regionsFilledWithTransparent";
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColorOrTransparent) {
        return "regionsFilledWithSpecificColorOrTransparent";
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColor) {
        return "allRegionsExceptFilledWithSpecificColor";
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithTransparent) {
        return "allRegionsExceptFilledWithTransparent";
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColorOrTransparent) {
        return "allRegionsExceptFilledWithSpecificColorOrTransparent";
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedBySpecificColor) {
        return "regionsSurroundedBySpecificColor";
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedByTransparent) {
        return "regionsSurroundedByTransparent";
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedBySpecificColorOrTransparent) {
        return "regionsSurroundedBySpecificColorOrTransparent";
    }
    return QString();
}

KisToolEncloseAndFill::RegionSelectionMethod KisToolEncloseAndFill::configStringToRegionSelectionMethod(const QString &configString) const
{
    if (configString == "regionsFilledWithSpecificColor") {
        return RegionSelectionMethod::SelectRegionsFilledWithSpecificColor;
    } else if (configString == "regionsFilledWithTransparent") {
        return RegionSelectionMethod::SelectRegionsFilledWithTransparent;
    } else if (configString == "regionsFilledWithSpecificColorOrTransparent") {
        return RegionSelectionMethod::SelectRegionsFilledWithSpecificColorOrTransparent;
    } else if (configString == "allRegionsExceptFilledWithSpecificColor") {
        return RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColor;
    } else if (configString == "allRegionsExceptFilledWithTransparent") {
        return RegionSelectionMethod::SelectAllRegionsExceptFilledWithTransparent;
    } else if (configString == "allRegionsExceptFilledWithSpecificColorOrTransparent") {
        return RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColorOrTransparent;
    } else if (configString == "regionsSurroundedBySpecificColor") {
        return RegionSelectionMethod::SelectRegionsSurroundedBySpecificColor;
    } else if (configString == "regionsSurroundedByTransparent") {
        return RegionSelectionMethod::SelectRegionsSurroundedByTransparent;
    } else if (configString == "regionsSurroundedBySpecificColorOrTransparent") {
        return RegionSelectionMethod::SelectRegionsSurroundedBySpecificColorOrTransparent;
    }
    return RegionSelectionMethod::SelectAllRegions;
}

KoColor KisToolEncloseAndFill::loadRegionSelectionColorFromConfig()
{
    const QString xmlColor = m_configGroup.readEntry("regionSelectionColor", QString());
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

KisToolEncloseAndFill::Reference KisToolEncloseAndFill::loadReferenceFromConfig() const
{
    if (m_configGroup.hasKey("reference")) {
        return configStringToReference(m_configGroup.readEntry("reference", referenceToConfigString(defaultReference())));
    } else {
        bool sampleMerged = m_configGroup.readEntry("sampleMerged", false);
        return sampleMerged ? AllLayers : CurrentLayer;
    }
    return CurrentLayer;
}

void KisToolEncloseAndFill::saveReferenceToConfig(Reference reference)
{
    m_configGroup.writeEntry("reference", referenceToConfigString(reference));
}

QString KisToolEncloseAndFill::referenceToConfigString(Reference reference) const
{
    if (reference == AllLayers) {
        return "allLayers";
    } else if (reference == ColorLabeledLayers) {
        return "colorLabeledLayers";
    }
    return "currentLayer";
}

KisToolEncloseAndFill::Reference KisToolEncloseAndFill::configStringToReference(const QString &configString) const
{
    if (configString == "allLayers") {
        return AllLayers;
    } else if (configString == "colorLabeledLayers") {
        return ColorLabeledLayers;
    }
    return CurrentLayer;
}

void KisToolEncloseAndFill::slot_optionButtonStripEnclosingMethod_buttonToggled(
    KoGroupButton *button,
    bool checked)
{
    if (!checked) {
        return;
    }

    if (button == m_buttonEnclosingMethodRectangle) {
        m_enclosingMethod = Rectangle;
    } else if (button == m_buttonEnclosingMethodEllipse) {
        m_enclosingMethod = Ellipse;
    } else if (button == m_buttonEnclosingMethodPath) {
        m_enclosingMethod = Path;
    } else if (button == m_buttonEnclosingMethodLasso) {
        m_enclosingMethod = Lasso;
    } else {
        m_enclosingMethod = Brush;
    }

    saveEnclosingMethodToConfig(m_enclosingMethod);
    setupEnclosingSubtool();
}

void KisToolEncloseAndFill::slot_comboBoxRegionSelectionMethod_currentIndexChanged(int)
{
    m_regionSelectionMethod = static_cast<RegionSelectionMethod>(m_comboBoxRegionSelectionMethod->currentData().toInt());

    KisOptionCollectionWidgetWithHeader *sectionWhatToFill =
        m_optionWidget->widgetAs<KisOptionCollectionWidgetWithHeader*>("sectionWhatToFill");
    sectionWhatToFill->setWidgetVisible("buttonRegionSelectionColor",
        m_regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColor ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColorOrTransparent ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColor ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColorOrTransparent ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedBySpecificColor ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedBySpecificColorOrTransparent
    );
    sectionWhatToFill->setWidgetVisible(
        "checkBoxRegionSelectionIncludeContourRegions",
        m_regionSelectionMethod == RegionSelectionMethod::SelectAllRegions ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColor ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithTransparent ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColorOrTransparent ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColor ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithTransparent ||
        m_regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColorOrTransparent
    );

    m_comboBoxRegionSelectionMethod->setToolTip(m_comboBoxRegionSelectionMethod->currentText());

    saveRegionSelectionMethodToConfig(m_regionSelectionMethod);
}

void KisToolEncloseAndFill::slot_buttonRegionSelectionColor_changed(const KoColor &color)
{
    if (color == m_regionSelectionColor) {
        return;
    }
    m_regionSelectionColor = color;
    m_configGroup.writeEntry("regionSelectionColor", color.toXML());
}

void KisToolEncloseAndFill::slot_checkBoxRegionSelectionInvert_toggled(bool checked)
{
    if (checked == m_regionSelectionInvert) {
        return;
    }
    m_regionSelectionInvert = checked;
    m_configGroup.writeEntry("regionSelectionInvert", checked);
}

void KisToolEncloseAndFill::slot_checkBoxRegionSelectionIncludeContourRegions_toggled(bool checked)
{
    if (checked == m_regionSelectionIncludeContourRegions) {
        return;
    }
    m_regionSelectionIncludeContourRegions = checked;
    m_configGroup.writeEntry("regionSelectionIncludeContourRegions", checked);
}

void KisToolEncloseAndFill::slot_optionButtonStripFillWith_buttonToggled(
    KoGroupButton *button,
    bool checked)
{
    if (!checked) {
        return;
    }
    const bool visible = button == m_buttonFillWithPattern;
    KisOptionCollectionWidgetWithHeader *sectionFillWith =
        m_optionWidget->widgetAs<KisOptionCollectionWidgetWithHeader*>("sectionFillWith");
    sectionFillWith->setWidgetVisible("sliderPatternScale", visible);
    sectionFillWith->setWidgetVisible("angleSelectorPatternRotation", visible);
    
    m_fillType = button == m_buttonFillWithFG ? FillWithForegroundColor
                                              : (button == m_buttonFillWithBG ? FillWithBackgroundColor : FillWithPattern);

    m_configGroup.writeEntry(
        "fillWith",
        button == m_buttonFillWithFG ? "foregroundColor" : (button == m_buttonFillWithBG ? "backgroundColor" : "pattern")
    );
}

void KisToolEncloseAndFill::slot_sliderPatternScale_valueChanged(double value)
{
    if (value == m_patternScale) {
        return;
    }
    m_patternScale = value;
    m_configGroup.writeEntry("patternScale", value);
}

void KisToolEncloseAndFill::slot_angleSelectorPatternRotation_angleChanged(double value)
{
    if (value == m_patternRotation) {
        return;
    }
    m_patternRotation = value;
    m_configGroup.writeEntry("patternRotate", value);
}

void KisToolEncloseAndFill::slot_checkBoxUseCustomBlendingOptions_toggled(bool checked)
{
    KisOptionCollectionWidgetWithHeader *sectionFillWith =
        m_optionWidget->widgetAs<KisOptionCollectionWidgetWithHeader*>("sectionFillWith");
    sectionFillWith->setWidgetVisible("sliderCustomOpacity", checked);
    sectionFillWith->setWidgetVisible("comboBoxCustomCompositeOp", checked);
    m_useCustomBlendingOptions = checked;
    m_configGroup.writeEntry("useCustomBlendingOptions", checked);
}

void KisToolEncloseAndFill::slot_sliderCustomOpacity_valueChanged(int value)
{
    if (value == m_customOpacity) {
        return;
    }
    m_customOpacity = value;
    m_configGroup.writeEntry("customOpacity", value);
}

void KisToolEncloseAndFill::slot_comboBoxCustomCompositeOp_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    const QString compositeOpId = m_comboBoxCustomCompositeOp->selectedCompositeOp().id();
    if (compositeOpId == m_customCompositeOp) {
        return;
    }
    m_customCompositeOp = compositeOpId;
    m_configGroup.writeEntry("customCompositeOp", compositeOpId);
}

void KisToolEncloseAndFill::slot_sliderFillThreshold_valueChanged(int value)
{
    if (value == m_fillThreshold) {
        return;
    }
    m_fillThreshold = value;
    m_configGroup.writeEntry("fillThreshold", value);
}

void KisToolEncloseAndFill::slot_sliderFillOpacitySpread_valueChanged(int value)
{
    if (value == m_fillOpacitySpread) {
        return;
    }
    m_fillOpacitySpread = value;
    m_configGroup.writeEntry("fillOpacitySpread", value);
}

void KisToolEncloseAndFill::slot_sliderCloseGap_valueChanged(int value)
{
    if (value == m_closeGap) {
        return;
    }
    m_closeGap = value;
    m_configGroup.writeEntry("closeGapAmount", value);
}

void KisToolEncloseAndFill::slot_checkBoxSelectionAsBoundary_toggled(bool checked)
{
    if (checked == m_useSelectionAsBoundary) {
        return;
    }
    m_useSelectionAsBoundary = checked;
    m_configGroup.writeEntry("useSelectionAsBoundary", checked);
}

void KisToolEncloseAndFill::slot_checkBoxAntiAlias_toggled(bool checked)
{
    if (checked == m_antiAlias) {
        return;
    }
    m_antiAlias = checked;
    m_configGroup.writeEntry("antiAlias", checked);
}

void KisToolEncloseAndFill::slot_sliderExpand_valueChanged(int value)
{
    if (value == m_expand) {
        return;
    }
    m_expand = value;
    m_configGroup.writeEntry("expand", value);
}

void KisToolEncloseAndFill::slot_buttonStopGrowingAtDarkestPixel_toggled(bool enabled)
{
    if (enabled == m_stopGrowingAtDarkestPixel) {
        return;
    }
    m_stopGrowingAtDarkestPixel = enabled;
    m_configGroup.writeEntry("stopGrowingAtDarkestPixel", enabled);
}

void KisToolEncloseAndFill::slot_sliderFeather_valueChanged(int value)
{
    if (value == m_feather) {
        return;
    }
    m_feather = value;
    m_configGroup.writeEntry("feather", value);
}

void KisToolEncloseAndFill::slot_optionButtonStripReference_buttonToggled(
    KoGroupButton *button,
    bool checked)
{
    if (!checked) {
        return;
    }
    KisOptionCollectionWidgetWithHeader *sectionReference =
        m_optionWidget->widgetAs<KisOptionCollectionWidgetWithHeader*>("sectionReference");
    sectionReference->setWidgetVisible("widgetLabels", button == m_buttonReferenceLabeled);
    
    m_reference = button == m_buttonReferenceCurrent ? CurrentLayer
                                                     : (button == m_buttonReferenceAll ? AllLayers : ColorLabeledLayers);

    m_configGroup.writeEntry(
        "reference",
        button == m_buttonReferenceCurrent ? "currentLayer" : (button == m_buttonReferenceAll ? "allLayers" : "colorLabeledLayers")
    );
}

void KisToolEncloseAndFill::slot_widgetLabels_selectionChanged()
{
    QList<int> labels = m_widgetLabels->selection();
    if (labels == m_selectedColorLabels) {
        return;
    }
    m_selectedColorLabels = labels;
    if (labels.isEmpty()) {
        return;
    }
    QString colorLabels = QString::number(labels.first());
    for (int i = 1; i < labels.size(); ++i) {
        colorLabels += "," + QString::number(labels[i]);
    }
    m_configGroup.writeEntry("colorLabels", colorLabels);
}

void KisToolEncloseAndFill::slot_buttonReset_clicked()
{
    m_buttonEnclosingMethodLasso->setChecked(true);
    m_comboBoxRegionSelectionMethod->setCurrentIndex(
        m_comboBoxRegionSelectionMethod->findData(static_cast<int>(RegionSelectionMethod::SelectAllRegions))
    );
    m_buttonRegionSelectionColor->setColor(KoColor());
    m_checkBoxRegionSelectionInvert->setChecked(false);
    m_checkBoxRegionSelectionIncludeContourRegions->setChecked(false);
    m_buttonFillWithFG->setChecked(true);
    m_sliderPatternScale->setValue(100.0);
    m_angleSelectorPatternRotation->setAngle(0.0);
    m_checkBoxCustomBlendingOptions->setChecked(false);
    m_sliderCustomOpacity->setValue(100);
    m_comboBoxCustomCompositeOp->selectCompositeOp(KoID(COMPOSITE_OVER));
    m_sliderFillThreshold->setValue(8);
    m_sliderFillOpacitySpread->setValue(100);
    m_sliderCloseGap->setValue(0);
    m_checkBoxSelectionAsBoundary->setChecked(true);
    m_checkBoxAntiAlias->setChecked(false);
    m_sliderExpand->setValue(0);
    m_buttonStopGrowingAtDarkestPixel->setChecked(false);
    m_sliderFeather->setValue(0);
    m_buttonReferenceCurrent->setChecked(true);
    m_widgetLabels->setSelection({});
}

void KisToolEncloseAndFill::slot_currentNodeChanged(const KisNodeSP node)
{
    if (m_previousNode && m_previousNode->paintDevice()) {
        disconnect(m_previousNode->paintDevice().data(),
                   SIGNAL(colorSpaceChanged(const KoColorSpace*)),
                   this,
                   SLOT(slot_colorSpaceChanged(const KoColorSpace*)));
    }
    if (node && node->paintDevice()) {
        connect(node->paintDevice().data(),
                SIGNAL(colorSpaceChanged(const KoColorSpace*)),
                this,
                SLOT(slot_colorSpaceChanged(const KoColorSpace*)));
        slot_colorSpaceChanged(node->paintDevice()->colorSpace());
    }
    m_previousNode = node;
}

void KisToolEncloseAndFill::slot_colorSpaceChanged(const KoColorSpace *colorSpace)
{
    if (!m_comboBoxCustomCompositeOp) {
        return;
    }
    const KoColorSpace *compositionSpace = colorSpace;
    if (currentNode() && currentNode()->paintDevice()) {
        // Currently, composition source is enough to determine the available blending mode,
        // because either destination is the same (paint layers), or composition happens
        // in source space (masks).
        compositionSpace = currentNode()->paintDevice()->compositionSourceColorSpace();
    }
    m_comboBoxCustomCompositeOp->validate(compositionSpace);
}

void KisToolEncloseAndFill::slot_checkBoxUseActiveLayer_toggled(bool checked)
{
    if (checked == m_useActiveLayer) {
        return;
    }
    m_useActiveLayer = checked;
    m_configGroup.writeEntry("useActiveLayer", checked);
}
