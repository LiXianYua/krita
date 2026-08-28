/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_painting_assistant_collection.h"

#include <utility>

KisPaintingAssistantCollection::KisPaintingAssistantCollection(
    const PkList<KisPaintingAssistantSP> &assistants)
    : m_assistants(assistants)
{
}

PkList<KisPaintingAssistantSP> KisPaintingAssistantCollection::assistants() const
{
    return m_assistants;
}

void KisPaintingAssistantCollection::setAssistants(
    const PkList<KisPaintingAssistantSP> &assistants)
{
    m_assistants = assistants;
    if (m_firstAssistant && !m_assistants.contains(m_firstAssistant)) {
        m_firstAssistant.clear();
    }
}

KisPaintingAssistantSP KisPaintingAssistantCollection::firstAssistant() const
{
    return m_firstAssistant;
}

void KisPaintingAssistantCollection::setFirstAssistant(KisPaintingAssistantSP assistant)
{
    m_firstAssistant = assistant;
}

void KisPaintingAssistantCollection::endStroke()
{
    m_firstAssistant.clear();

    for (const KisPaintingAssistantSP &assistant : std::as_const(m_assistants)) {
        assistant->endStroke();
    }
}
