/*
 * SPDX-FileCopyrightText: 2008, 2011 Cyrille Berger <cberger@cberger.net>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_painting_assistant.h"

KisPaintingAssistantFactory::KisPaintingAssistantFactory() = default;

KisPaintingAssistantFactory::~KisPaintingAssistantFactory() = default;

KisPaintingAssistantFactoryRegistry::KisPaintingAssistantFactoryRegistry() = default;

KisPaintingAssistantFactoryRegistry::~KisPaintingAssistantFactoryRegistry()
{
    for (const PkString &id : keys()) {
        delete get(id);
    }
}

KisPaintingAssistantFactoryRegistry *KisPaintingAssistantFactoryRegistry::instance()
{
    static KisPaintingAssistantFactoryRegistry registry;
    return &registry;
}
