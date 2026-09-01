/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <kis_debug.h>
#include <klocalizedstring.h>

#include <ksharedconfig.h>

#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoPointerEvent.h>

#include <kis_layer.h>
#include <kis_painter.h>
#include <resources/KoPattern.h>
#include <kis_selection.h>

#include <KisCanvasFeedback.h>
#include "kis_resources_snapshot.h"
#include <kis_image_animation_interface.h>

#include <kis_stroke_strategy_undo_command_based.h>
#include <commands_new/kis_processing_command.h>
#include <commands_new/kis_update_command.h>
#include <kis_command_utils.h>
#include <functional>
#include <PkXmlDocument.h>
#include <PkStringList.h>
#include <PkTransform.h>
#include <kis_group_layer.h>
#include <kis_layer_utils.h>

#include <kis_dummies_facade.h>
#include <KoShapeControllerBase.h>
#include <kis_shape_controller.h>

#include <KoCompositeOpRegistry.h>

#include <processing/KisEncloseAndFillProcessingVisitor.h>

#include "KisToolEncloseAndFill.h"
#include "subtools/KisRectangleEnclosingProducer.h"
#include "subtools/KisEllipseEnclosingProducer.h"
#include "subtools/KisPathEnclosingProducer.h"
#include "subtools/KisLassoEnclosingProducer.h"
#include "subtools/KisBrushEnclosingProducer.h"

KisToolEncloseAndFill::KisToolEncloseAndFill(KoCanvasBase * canvas)
    : KisDynamicDelegatedTool<KisToolShape>(canvas)
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

void KisToolEncloseAndFill::activate(const PkSet<KoShape*> &shapes)
{
    KisDynamicDelegatedTool::activate(shapes);
    m_configGroup = KSharedConfig::openConfig()->group(toolId());

    // Was only called from createOptionWidget() (now deleted), which ran on
    // every tool activation, so this keeps the same effective defaults
    // (config-driven, via the same m_configGroup keys) without a panel.
    loadConfiguration();

    KoCanvasResourceProvider *resourceProvider = canvas()->resourceManager();
    if (resourceProvider) {
        connect(resourceProvider,
                &KoCanvasResourceProvider::canvasResourceChanged,
                this,
                &KisToolEncloseAndFill::slot_canvasResourceChanged,
                PkConnectionType::Unique);
        slot_currentNodeChanged(currentNode());
    }
}

void KisToolEncloseAndFill::deactivate()
{
    m_referencePaintDevice = nullptr;
    m_referenceNodeList = nullptr;
    KoCanvasResourceProvider *resourceProvider = canvas()->resourceManager();
    if (resourceProvider) {
        disconnect(resourceProvider,
                   &KoCanvasResourceProvider::canvasResourceChanged,
                   this,
                   &KisToolEncloseAndFill::slot_canvasResourceChanged);
    }
    slot_currentNodeChanged(nullptr);
    KisDynamicDelegatedTool::deactivate();
}

void KisToolEncloseAndFill::setupEnclosingSubtool()
{
    if (delegateTool()) {
        delegateTool()->deactivate();
    }

    const auto installDelegate = [this](auto *newDelegateTool) {
        using Producer = std::remove_pointer_t<decltype(newDelegateTool)>;
        setDelegateTool(reinterpret_cast<KisDynamicDelegateTool<KisToolShape>*>(newDelegateTool));
        setCursor(newDelegateTool->cursor());
        connect(newDelegateTool,
                &Producer::enclosingMaskProduced,
                this,
                &KisToolEncloseAndFill::slot_delegateTool_enclosingMaskProduced);
    };

    if (m_enclosingMethod == Ellipse) {
        KisEllipseEnclosingProducer *newDelegateTool = new KisEllipseEnclosingProducer(canvas());
        installDelegate(newDelegateTool);
    } else if (m_enclosingMethod == Path) {
        KisPathEnclosingProducer *newDelegateTool = new KisPathEnclosingProducer(canvas());
        installDelegate(newDelegateTool);
    } else if (m_enclosingMethod == Lasso) {
        KisLassoEnclosingProducer *newDelegateTool = new KisLassoEnclosingProducer(canvas());
        installDelegate(newDelegateTool);
    } else if (m_enclosingMethod == Brush) {
        KisBrushEnclosingProducer *newDelegateTool = new KisBrushEnclosingProducer(canvas());
        installDelegate(newDelegateTool);
    } else {
        KisRectangleEnclosingProducer *newDelegateTool = new KisRectangleEnclosingProducer(canvas());
        installDelegate(newDelegateTool);
    }

    if (isActivated()) {
        delegateTool()->activate(PkSet<KoShape*>());
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
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
        feedback->showFloatingMessage(
            PkString("You cannot use this tool with the selected layer type"),
            {}, 2000, KisCanvasFeedback::Priority::Medium, Qt::AlignCenter);
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

    m_dirtyRect.reset(new PkRect);

    KisResourcesSnapshotSP resources(
        new KisResourcesSnapshot(image(), currentNode(), this->canvas()->resourceManager()->canvasResourcesInterface()));

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

    PkTransform transform;
    transform.rotate(m_patternRotation);
    const qreal normalizedScale = m_patternScale * 0.01;
    transform.scale(normalizedScale, normalizedScale);
    resources->setFillTransform(transform);

    KisProcessingVisitorSP visitor(
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
                                               m_dirtyRect));

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
        const PkString fillTypeStr = m_configGroup.readEntry<PkString>("fillWith", "");
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
    m_customCompositeOp = m_configGroup.readEntry<PkString>("customCompositeOp", COMPOSITE_OVER);
    if (KoCompositeOpRegistry::instance().getKoID(m_customCompositeOp).id().isEmpty()) {
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
        const PkString sampleLayersModeStr = m_configGroup.readEntry<PkString>("reference", "currentLayer");
        if (sampleLayersModeStr == "allLayers") {
            m_reference = AllLayers;
        } else if (sampleLayersModeStr == "colorLabeledLayers") {
            m_reference = ColorLabeledLayers;
        } else {
            m_reference = CurrentLayer;
        }
    }
    {
        const auto colorLabelsStr = m_configGroup.readEntry<PkString>("colorLabels", "").split(',');

        m_selectedColorLabels.clear();
        for (const PkString &colorLabelStr : colorLabelsStr) {
            if (colorLabelStr.isEmpty()) {
                continue;
            }
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

PkString KisToolEncloseAndFill::enclosingMethodToConfigString(EnclosingMethod enclosingMethod) const
{
    switch (enclosingMethod) {
        case Rectangle: return "rectangle";
        case Ellipse: return "ellipse";
        case Path: return "path";
        case Brush: return "brush";
        default: return "lasso";
    }
}

KisToolEncloseAndFill::EnclosingMethod KisToolEncloseAndFill::configStringToEnclosingMethod(const PkString &configString) const
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

PkString KisToolEncloseAndFill::regionSelectionMethodToUserString(RegionSelectionMethod regionSelectionMethod) const
{
    if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegions) {
        return PkString("All");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColor) {
        return PkString("Specific color");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithTransparent) {
        return PkString("Transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsFilledWithSpecificColorOrTransparent) {
        return PkString("Specific color or transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColor) {
        return PkString("All, excluding a specific color");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithTransparent) {
        return PkString("All, excluding transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectAllRegionsExceptFilledWithSpecificColorOrTransparent) {
        return PkString("All, excluding a specific color or transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedBySpecificColor) {
        return PkString("Any surrounded by a specific color");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedByTransparent) {
        return PkString("Any surrounded by transparency");
    } else if (regionSelectionMethod == RegionSelectionMethod::SelectRegionsSurroundedBySpecificColorOrTransparent) {
        return PkString("Any surrounded by a specific color or transparency");
    }
    return PkString();
}

KisToolEncloseAndFill::RegionSelectionMethod KisToolEncloseAndFill::loadRegionSelectionMethodFromConfig() const
{
    return configStringToRegionSelectionMethod(m_configGroup.readEntry("regionSelectionMethod", regionSelectionMethodToConfigString(defaultRegionSelectionMethod())));
}

void KisToolEncloseAndFill::saveRegionSelectionMethodToConfig(RegionSelectionMethod regionSelectionMethod)
{
    m_configGroup.writeEntry("regionSelectionMethod", regionSelectionMethodToConfigString(regionSelectionMethod));
}

PkString KisToolEncloseAndFill::regionSelectionMethodToConfigString(RegionSelectionMethod regionSelectionMethod) const
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
    return PkString();
}

KisToolEncloseAndFill::RegionSelectionMethod KisToolEncloseAndFill::configStringToRegionSelectionMethod(const PkString &configString) const
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
    const PkString xmlColor = m_configGroup.readEntry("regionSelectionColor", PkString());
    PkXmlDocument doc;
    if (doc.setContent(xmlColor)) {
        PkXmlElement e = doc.documentElement().firstChild().toElement();
        PkString channelDepthID = doc.documentElement().attribute("channeldepth", Integer16BitsColorDepthID.id());
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

PkString KisToolEncloseAndFill::referenceToConfigString(Reference reference) const
{
    if (reference == AllLayers) {
        return "allLayers";
    } else if (reference == ColorLabeledLayers) {
        return "colorLabeledLayers";
    }
    return "currentLayer";
}

KisToolEncloseAndFill::Reference KisToolEncloseAndFill::configStringToReference(const PkString &configString) const
{
    if (configString == "allLayers") {
        return AllLayers;
    } else if (configString == "colorLabeledLayers") {
        return ColorLabeledLayers;
    }
    return CurrentLayer;
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

void KisToolEncloseAndFill::slot_sliderCustomOpacity_valueChanged(int value)
{
    if (value == m_customOpacity) {
        return;
    }
    m_customOpacity = value;
    m_configGroup.writeEntry("customOpacity", value);
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

void KisToolEncloseAndFill::slot_currentNodeChanged(const KisNodeSP node)
{
    if (m_previousNode && m_previousNode->paintDevice()) {
        disconnect(m_previousNode->paintDevice().data(),
                   &KisPaintDevice::colorSpaceChanged,
                   this,
                   &KisToolEncloseAndFill::slot_colorSpaceChanged);
    }
    if (node && node->paintDevice()) {
        connect(node->paintDevice().data(),
                &KisPaintDevice::colorSpaceChanged,
                this,
                &KisToolEncloseAndFill::slot_colorSpaceChanged);
        slot_colorSpaceChanged(node->paintDevice()->colorSpace());
    }
    m_previousNode = node;
}

void KisToolEncloseAndFill::slot_canvasResourceChanged(int key, const PkVariant &value)
{
    if (key == KoCanvasResource::CurrentKritaNode) {
        slot_currentNodeChanged(value.value<KisNodeWSP>());
    }
}

void KisToolEncloseAndFill::slot_colorSpaceChanged(const KoColorSpace *colorSpace)
{
    (void)colorSpace;
    // Was forwarded to the options panel's composite-op combobox (grey out
    // composite ops unsupported by the current color space); the panel has
    // been removed, nothing left to validate against. Still connected from
    // slot_currentNodeChanged() on every real node/color-space change --
    // kept as a documented no-op rather than removing that live connection.
}

void KisToolEncloseAndFill::slot_checkBoxUseActiveLayer_toggled(bool checked)
{
    if (checked == m_useActiveLayer) {
        return;
    }
    m_useActiveLayer = checked;
    m_configGroup.writeEntry("useActiveLayer", checked);
}
