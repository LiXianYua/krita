/*
 *  SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_selection_options.h"

KisSelectionOptions::KisSelectionOptions(PkObject *parent)
    : PkObject(parent)
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

PkList<int> KisSelectionOptions::selectedColorLabels() const
{
    return m_selectedColorLabels;
}

void KisSelectionOptions::setMode(SelectionMode value)
{
    if (m_mode == value) return;
    m_mode = value;
    modeChanged(value);
}

void KisSelectionOptions::setAction(SelectionAction value)
{
    if (m_action == value) return;
    m_action = value;
    actionChanged(value);
}

void KisSelectionOptions::setAntiAliasSelection(bool value)
{
    if (m_antiAliasSelection == value) return;
    m_antiAliasSelection = value;
    antiAliasSelectionChanged(value);
}

void KisSelectionOptions::setGrowSelection(int value)
{
    if (m_growSelection == value) return;
    m_growSelection = value;
    growSelectionChanged(value);
}

void KisSelectionOptions::setStopGrowingAtDarkestPixel(bool value)
{
    if (m_stopGrowingAtDarkestPixel == value) return;
    m_stopGrowingAtDarkestPixel = value;
    stopGrowingAtDarkestPixelChanged(value);
}

void KisSelectionOptions::setFeatherSelection(int value)
{
    if (m_featherSelection == value) return;
    m_featherSelection = value;
    featherSelectionChanged(value);
}

void KisSelectionOptions::setReferenceLayers(ReferenceLayers value)
{
    if (m_referenceLayers == value) return;
    m_referenceLayers = value;
    referenceLayersChanged(value);
}

void KisSelectionOptions::setSelectedColorLabels(const PkList<int> &value)
{
    if (m_selectedColorLabels == value) return;
    m_selectedColorLabels = value;
    selectedColorLabelsChanged();
}
