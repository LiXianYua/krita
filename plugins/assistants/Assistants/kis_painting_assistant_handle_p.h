/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PAINTING_ASSISTANT_HANDLE_P_H
#define KIS_PAINTING_ASSISTANT_HANDLE_P_H

#include "kis_painting_assistant.h"

struct KisPaintingAssistantHandle::Private {
    QList<KisPaintingAssistant*> assistants;
    char handleType {HandleType::NORMAL};
};

#endif
