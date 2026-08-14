/*
 *  SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_selection_options.h"

KisSelectionOptions::KisSelectionOptions(QObject *parent)
    : QObject(parent)
{
}

SelectionMode KisSelectionOptions::mode() const
{
    return m_mode;
}

SelectionAction KisSelectionOptions::action() const
{
    return m_action;
}

bool KisSelectionOptions::antiAliasSelection() const
{
    return m_antiAliasSelection;
}

int KisSelectionOptions::growSelection() const
{
    return m_growSelection;
}

bool KisSelectionOptions::stopGrowingAtDarkestPixel() const
{
    return m_stopGrowingAtDarkestPixel;
}

int KisSelectionOptions::featherSelection() const
{
    return m_featherSelection;
}

KisSelectionOptions::ReferenceLayers KisSelectionOptions::referenceLayers() const
{
    return m_referenceLayers;
}

QList<int> KisSelectionOptions::selectedColorLabels() const
{
    return m_selectedColorLabels;
}

void KisSelectionOptions::setMode(SelectionMode value)
{
    if (m_mode == value) return;
    m_mode = value;
    Q_EMIT modeChanged(value);
}

void KisSelectionOptions::setAction(SelectionAction value)
{
    if (m_action == value) return;
    m_action = value;
    Q_EMIT actionChanged(value);
}

void KisSelectionOptions::setAntiAliasSelection(bool value)
{
    if (m_antiAliasSelection == value) return;
    m_antiAliasSelection = value;
    Q_EMIT antiAliasSelectionChanged(value);
}

void KisSelectionOptions::setGrowSelection(int value)
{
    if (m_growSelection == value) return;
    m_growSelection = value;
    Q_EMIT growSelectionChanged(value);
}

void KisSelectionOptions::setStopGrowingAtDarkestPixel(bool value)
{
    if (m_stopGrowingAtDarkestPixel == value) return;
    m_stopGrowingAtDarkestPixel = value;
    Q_EMIT stopGrowingAtDarkestPixelChanged(value);
}

void KisSelectionOptions::setFeatherSelection(int value)
{
    if (m_featherSelection == value) return;
    m_featherSelection = value;
    Q_EMIT featherSelectionChanged(value);
}

void KisSelectionOptions::setReferenceLayers(ReferenceLayers value)
{
    if (m_referenceLayers == value) return;
    m_referenceLayers = value;
    Q_EMIT referenceLayersChanged(value);
}

void KisSelectionOptions::setSelectedColorLabels(const QList<int> &value)
{
    if (m_selectedColorLabels == value) return;
    m_selectedColorLabels = value;
    Q_EMIT selectedColorLabelsChanged();
}
