/*
 * SPDX-FileCopyrightText: 2008, 2011 Cyrille Berger <cberger@cberger.net>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_painting_assistant.h"

#include "kis_debug.h"

#include <QGlobalStatic>

Q_GLOBAL_STATIC(KisPaintingAssistantFactoryRegistry, s_instance)

KisPaintingAssistantFactory::KisPaintingAssistantFactory() = default;

KisPaintingAssistantFactory::~KisPaintingAssistantFactory() = default;

KisPaintingAssistantFactoryRegistry::KisPaintingAssistantFactoryRegistry() = default;

KisPaintingAssistantFactoryRegistry::~KisPaintingAssistantFactoryRegistry()
{
    Q_FOREACH (const QString &id, keys()) {
        delete get(id);
    }
    dbgRegistry << "deleting KisPaintingAssistantFactoryRegistry ";
}

KisPaintingAssistantFactoryRegistry *KisPaintingAssistantFactoryRegistry::instance()
{
    return s_instance;
}
