/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_selection_tool_config_widget_helper.h"

#include "kis_selection_options.h"
#include <kis_signals_blocker.h>

#include <KConfigGroup>
#include <KSharedConfig>

KisSelectionToolConfigWidgetHelper::KisSelectionToolConfigWidgetHelper(
    const QString &windowTitle)
    : m_windowTitle(windowTitle)
{
    connect(&m_options, &KisSelectionOptions::modeChanged,
            this, &KisSelectionToolConfigWidgetHelper::slotWidgetModeChanged);
    connect(&m_options,
            &KisSelectionOptions::actionChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotWidgetActionChanged);
    connect(&m_options, &KisSelectionOptions::antiAliasSelectionChanged,
            this, &KisSelectionToolConfigWidgetHelper::slotWidgetAntiAliasChanged);
    connect(&m_options,
            &KisSelectionOptions::growSelectionChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotWidgetGrowChanged);
    connect(&m_options,
            &KisSelectionOptions::stopGrowingAtDarkestPixelChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotWidgetStopGrowingAtDarkestPixelChanged);
    connect(&m_options,
            &KisSelectionOptions::featherSelectionChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotWidgetFeatherChanged);
    connect(&m_options,
            &KisSelectionOptions::referenceLayersChanged,
            this,
            &KisSelectionToolConfigWidgetHelper::slotReferenceLayersChanged);
    connect(&m_options, &KisSelectionOptions::selectedColorLabelsChanged,
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

QList<int> KisSelectionToolConfigWidgetHelper::selectedColorLabels() const
{
    return m_options.selectedColorLabels();
}

void KisSelectionToolConfigWidgetHelper::setConfigGroupForExactTool(
    QString toolId)
{
    m_configGroupForTool = toolId;
    reloadExactToolConfig();
}

void KisSelectionToolConfigWidgetHelper::slotWidgetModeChanged(
    SelectionMode mode)
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group("KisToolSelectBase");
    cfg.writeEntry("selectionMode", static_cast<int>(mode));
}

void KisSelectionToolConfigWidgetHelper::slotWidgetActionChanged(
    SelectionAction action)
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group("KisToolSelectBase");
    cfg.writeEntry("selectionAction", static_cast<int>(action));
    Q_EMIT selectionActionChanged(action);
}

void KisSelectionToolConfigWidgetHelper::slotWidgetAntiAliasChanged(bool value)
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry("antiAliasSelection", value);
}

void KisSelectionToolConfigWidgetHelper::slotWidgetGrowChanged(int value)
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry("growSelection", value);
}

void KisSelectionToolConfigWidgetHelper::slotWidgetStopGrowingAtDarkestPixelChanged(bool value)
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry("stopGrowingAtDarkestPixel", value);
}

void KisSelectionToolConfigWidgetHelper::slotWidgetFeatherChanged(int value)
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry("featherSelection", value);
}

void KisSelectionToolConfigWidgetHelper::slotReferenceLayersChanged(
    KisSelectionOptions::ReferenceLayers referenceLayers)
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group(m_configGroupForTool);
    cfg.writeEntry(
        "sampleLayersMode",
        referenceLayers == KisSelectionOptions::AllLayers
            ? "sampleAllLayers"
            : (referenceLayers == KisSelectionOptions::ColorLabeledLayers
                   ? "sampleColorLabeledLayers"
                   : "sampleCurrentLayer"));
}

void KisSelectionToolConfigWidgetHelper::slotSelectedColorLabelsChanged()
{
    const QList<int> colorLabels = m_options.selectedColorLabels();
    if (colorLabels.isEmpty()) {
        return;
    }
    QString colorLabelsStr = QString::number(colorLabels.first());
    for (int i = 1; i < colorLabels.size(); ++i) {
        colorLabelsStr += "," + QString::number(colorLabels[i]);
    }

    KConfigGroup cfg = KSharedConfig::openConfig()->group(m_configGroupForTool);
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

    KConfigGroup cfg = KSharedConfig::openConfig()->group("KisToolSelectBase");

    const SelectionMode selectionMode =
        (SelectionMode)cfg.readEntry("selectionMode",
                                     static_cast<int>(SHAPE_PROTECTION));
    const SelectionAction selectionAction =
        (SelectionAction)cfg.readEntry("selectionAction",
                                       static_cast<int>(SELECTION_REPLACE));

    KisSignalsBlocker b(&m_options);
    m_options.setMode(selectionMode);
    m_options.setAction(selectionAction);

    reloadExactToolConfig();
}

void KisSelectionToolConfigWidgetHelper::reloadExactToolConfig()
{
    if (m_configGroupForTool == "") {
        return;
    }

    KConfigGroup cfgToolSpecific =
        KSharedConfig::openConfig()->group(m_configGroupForTool);
    const bool antiAliasSelection =
        cfgToolSpecific.readEntry("antiAliasSelection", true);
    const int growSelection = cfgToolSpecific.readEntry("growSelection", 0);
    const bool stopGrowingAtDarkestPixel =
        cfgToolSpecific.readEntry("stopGrowingAtDarkestPixel", false);
    const int featherSelection =
        cfgToolSpecific.readEntry("featherSelection", 0);
    const QString referenceLayersStr =
        cfgToolSpecific.readEntry("sampleLayersMode", "sampleCurrentLayer");

    const QStringList colorLabelsStr =
        cfgToolSpecific.readEntry<QString>("colorLabels", "")
            .split(',', Qt::SkipEmptyParts);

    const KisSelectionOptions::ReferenceLayers referenceLayers =
        referenceLayersStr == "sampleAllLayers"
        ? KisSelectionOptions::AllLayers
        : (referenceLayersStr == "sampleColorLabeledLayers"
               ? KisSelectionOptions::ColorLabeledLayers
               : KisSelectionOptions::CurrentLayer);
    QList<int> colorLabels;
    for (const QString &colorLabelStr : colorLabelsStr) {
        bool ok;
        const int colorLabel = colorLabelStr.toInt(&ok);
        if (ok) {
            colorLabels << colorLabel;
        }
    }

    KisSignalsBlocker b(&m_options);
    m_options.setAntiAliasSelection(antiAliasSelection);
    m_options.setGrowSelection(growSelection);
    m_options.setStopGrowingAtDarkestPixel(stopGrowingAtDarkestPixel);
    m_options.setFeatherSelection(featherSelection);
    m_options.setReferenceLayers(referenceLayers);
    m_options.setSelectedColorLabels(colorLabels);
}
