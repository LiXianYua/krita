/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PAINTING_ASSISTANT_COLLECTION_H
#define KIS_PAINTING_ASSISTANT_COLLECTION_H

#include <QList>

#include <kritaassistanttool_export.h>
#include <kis_painting_assistant.h>

class KRITAASSISTANTTOOL_EXPORT KisPaintingAssistantCollection
{
public:
    explicit KisPaintingAssistantCollection(const QList<KisPaintingAssistantSP> &assistants = {});

    QList<KisPaintingAssistantSP> assistants() const;
    void setAssistants(const QList<KisPaintingAssistantSP> &assistants);

    KisPaintingAssistantSP firstAssistant() const;
    void setFirstAssistant(KisPaintingAssistantSP assistant);

    void endStroke();

private:
    QList<KisPaintingAssistantSP> m_assistants;
    KisPaintingAssistantSP m_firstAssistant;
};

#endif // KIS_PAINTING_ASSISTANT_COLLECTION_H
