/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PAINTING_ASSISTANT_COLLECTION_H
#define KIS_PAINTING_ASSISTANT_COLLECTION_H

#include <PkList.h>

#include <kritaassistanttool_export.h>
#include <kis_painting_assistant.h>

class KRITAASSISTANTTOOL_EXPORT KisPaintingAssistantCollection
{
public:
    explicit KisPaintingAssistantCollection(const PkList<KisPaintingAssistantSP> &assistants = {});

    PkList<KisPaintingAssistantSP> assistants() const;
    void setAssistants(const PkList<KisPaintingAssistantSP> &assistants);

    KisPaintingAssistantSP firstAssistant() const;
    void setFirstAssistant(KisPaintingAssistantSP assistant);

    void endStroke();

private:
    PkList<KisPaintingAssistantSP> m_assistants;
    KisPaintingAssistantSP m_firstAssistant;
};

#endif // KIS_PAINTING_ASSISTANT_COLLECTION_H
