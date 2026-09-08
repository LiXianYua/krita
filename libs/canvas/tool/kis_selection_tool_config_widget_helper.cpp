/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_selection_tool_config_widget_helper.h"

#include "kis_selection_options.h"
#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

KisSelectionToolConfigWidgetHelper::KisSelectionToolConfigWidgetHelper(
    const PkString &windowTitle)
    : m_windowTitle(windowTitle)
{
    PkObject::connect(&m_options, &KisSelectionOptions::modeChanged,
            this, &KisSelectionToolConfigWidgetHelper::slotWidgetModeChanged);
    PkObject::connect(&m_options,
            &KisSelectionOptions::actionChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotWidgetActionChanged);
    PkObject::connect(&m_options, &KisSelectionOptions::antiAliasSelectionChanged,
            this, &KisSelectionToolConfigWidgetHelper::slotWidgetAntiAliasChanged);
    PkObject::connect(&m_options,
            &KisSelectionOptions::growSelectionChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotWidgetGrowChanged);
    PkObject::connect(&m_options,
            &KisSelectionOptions::stopGrowingAtDarkestPixelChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotWidgetStopGrowingAtDarkestPixelChanged);
    PkObject::connect(&m_options,
            &KisSelectionOptions::featherSelectionChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotWidgetFeatherChanged);
    PkObject::connect(&m_options,
            &KisSelectionOptions::referenceLayersChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotReferenceLayersChanged);
    PkObject::connect(&m_options, &KisSelectionOptions::selectedColorLabelsChanged,
            this, &KisSelectionToolConfigWidgetHelper::slotSelectedColorLabelsChanged);

    slotToolActivatedChanged(true);
}

SelectionMode KisSelectionToolConfigWidgetHelper::selectionMode() const
{
    return m_options.mode();
}

SelectionAction KisSelectionToolConfigWidgetHelper::selectionAction() const
{
    return m_options.action();
}

bool KisSelectionToolConfigWidgetHelper::antiAliasSelection() const
{
    return m_options.antiAliasSelection();
}

int KisSelectionToolConfigWidgetHelper::growSelection() const
{
    return m_options.growSelection();
}

bool KisSelectionToolConfigWidgetHelper::stopGrowingAtDarkestPixel() const
{
    return m_options.stopGrowingAtDarkestPixel();
}

int KisSelectionToolConfigWidgetHelper::featherSelection() const
{
    return m_options.featherSelection();
}

KisSelectionOptions::ReferenceLayers
KisSelectionToolConfigWidgetHelper::referenceLayers() const
{
    return m_options.referenceLayers();
}

PkList<int> KisSelectionToolConfigWidgetHelper::selectedColorLabels() const
{
    return m_options.selectedColorLabels();
}

void KisSelectionToolConfigWidgetHelper::setConfigGroupForExactTool(
    const PkString &toolId)
{
    m_configGroupForTool = toolId;
    reloadExactToolConfig();
}

void KisSelectionToolConfigWidgetHelper::slotWidgetModeChanged(
    SelectionMode mode)
{
    if (m_loadingConfig) return;
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group("KisToolSelectBase");
    cfg.writeEntry("selectionMode", static_cast<int>(mode));
}

void KisSelectionToolConfigWidgetHelper::slotWidgetActionChanged(
    SelectionAction action)
{
    if (m_loadingConfig) return;
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group("KisToolSelectBase");
    cfg.writeEntry("selectionAction", static_cast<int>(action));
    selectionActionChanged(action);
}

void KisSelectionToolConfigWidgetHelper::slotWidgetAntiAliasChanged(bool value)
{
    if (m_loadingConfig) return;
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry("antiAliasSelection", value);
}

void KisSelectionToolConfigWidgetHelper::slotWidgetGrowChanged(int value)
{
    if (m_loadingConfig) return;
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry("growSelection", value);
}

void KisSelectionToolConfigWidgetHelper::slotWidgetStopGrowingAtDarkestPixelChanged(bool value)
{
    if (m_loadingConfig) return;
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry("stopGrowingAtDarkestPixel", value);
}

void KisSelectionToolConfigWidgetHelper::slotWidgetFeatherChanged(int value)
{
    if (m_loadingConfig) return;
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry("featherSelection", value);
}

void KisSelectionToolConfigWidgetHelper::slotReferenceLayersChanged(
    KisSelectionOptions::ReferenceLayers referenceLayers)
{
    if (m_loadingConfig) return;
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry(
        "sampleLayersMode",
        referenceLayers == KisSelectionOptions::AllLayers
            ? PkString("sampleAllLayers")
            : (referenceLayers == KisSelectionOptions::ColorLabeledLayers
                   ? PkString("sampleColorLabeledLayers")
                   : PkString("sampleCurrentLayer")));
}

void KisSelectionToolConfigWidgetHelper::slotSelectedColorLabelsChanged()
{
    if (m_loadingConfig) return;
    const PkList<int> colorLabels = m_options.selectedColorLabels();
    if (colorLabels.isEmpty()) {
        return;
    }
    PkString colorLabelsStr = PkString::number(colorLabels.first());
    for (int i = 1; i < colorLabels.size(); ++i) {
        colorLabelsStr += PkString(",") + PkString::number(colorLabels[i]);
    }

    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry("colorLabels", colorLabelsStr);
}

void KisSelectionToolConfigWidgetHelper::slotReplaceModeRequested()
{
    m_options.setAction(SELECTION_REPLACE);
}

void KisSelectionToolConfigWidgetHelper::slotAddModeRequested()
{
    m_options.setAction(SELECTION_ADD);
}

void KisSelectionToolConfigWidgetHelper::slotSubtractModeRequested()
{
    m_options.setAction(SELECTION_SUBTRACT);
}

void KisSelectionToolConfigWidgetHelper::slotIntersectModeRequested()
{
    m_options.setAction(SELECTION_INTERSECT);
}

void KisSelectionToolConfigWidgetHelper::slotSymmetricDifferenceModeRequested()
{
    m_options.setAction(SELECTION_SYMMETRICDIFFERENCE);
}

void KisSelectionToolConfigWidgetHelper::slotToolActivatedChanged(bool isActivated)
{
    if (!isActivated) {
        return;
    }

    PkConfigGroup cfg = PkSharedConfig::openConfig()->group("KisToolSelectBase");

    const SelectionMode selectionMode =
        (SelectionMode)cfg.readEntry("selectionMode",
                                     static_cast<int>(SHAPE_PROTECTION));
    const SelectionAction selectionAction =
        (SelectionAction)cfg.readEntry("selectionAction",
                                       static_cast<int>(SELECTION_REPLACE));

    m_loadingConfig = true;
    m_options.setMode(selectionMode);
    m_options.setAction(selectionAction);
    m_loadingConfig = false;

    reloadExactToolConfig();
}

void KisSelectionToolConfigWidgetHelper::reloadExactToolConfig()
{
    if (m_configGroupForTool == "") {
        return;
    }

    PkConfigGroup cfgToolSpecific =
        PkSharedConfig::openConfig()->group(m_configGroupForTool);
    const bool antiAliasSelection =
        cfgToolSpecific.readEntry("antiAliasSelection", true);
    const int growSelection = cfgToolSpecific.readEntry("growSelection", 0);
    const bool stopGrowingAtDarkestPixel =
        cfgToolSpecific.readEntry("stopGrowingAtDarkestPixel", false);
    const int featherSelection =
        cfgToolSpecific.readEntry("featherSelection", 0);
    const PkString referenceLayersStr =
        cfgToolSpecific.readEntry("sampleLayersMode", PkString("sampleCurrentLayer"));

    const PkList<PkString> colorLabelsStr =
        cfgToolSpecific.readEntry("colorLabels", PkString())
            .split(',', Pk::SkipEmptyParts);

    const KisSelectionOptions::ReferenceLayers referenceLayers =
        referenceLayersStr == "sampleAllLayers"
        ? KisSelectionOptions::AllLayers
        : (referenceLayersStr == "sampleColorLabeledLayers"
               ? KisSelectionOptions::ColorLabeledLayers
               : KisSelectionOptions::CurrentLayer);
    PkList<int> colorLabels;
    for (const PkString &colorLabelStr : colorLabelsStr) {
        bool ok;
        const int colorLabel = colorLabelStr.toInt(&ok);
        if (ok) {
            colorLabels << colorLabel;
        }
    }

    m_loadingConfig = true;
    m_options.setAntiAliasSelection(antiAliasSelection);
    m_options.setGrowSelection(growSelection);
    m_options.setStopGrowingAtDarkestPixel(stopGrowingAtDarkestPixel);
    m_options.setFeatherSelection(featherSelection);
    m_options.setReferenceLayers(referenceLayers);
    m_options.setSelectedColorLabels(colorLabels);
    m_loadingConfig = false;
}
