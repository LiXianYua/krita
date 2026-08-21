/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "generator/kis_generator_registry.h"

#include <math.h>

#include <PkString.h>

#include "filter/kis_filter_configuration.h"
#include "kis_debug.h"
#include "kis_types.h"
#include "kis_paint_device.h"
#include "generator/kis_generator.h"

KisGeneratorRegistry::KisGeneratorRegistry()
{
}

KisGeneratorRegistry::~KisGeneratorRegistry()
{
    for (KisGeneratorSP generator : values()) {
        remove(generator->id());
        generator.clear();
    }
    dbgRegistry << "deleting KisGeneratorRegistry";
}

KisGeneratorRegistry* KisGeneratorRegistry::instance()
{
    static KisGeneratorRegistry s_instance;
    return &s_instance;
}

void KisGeneratorRegistry::add(KisGeneratorSP item)
{
    dbgPlugins << "adding " << item->name();
    add(item->id(), item);
}

void KisGeneratorRegistry::add(const PkString &id, KisGeneratorSP item)
{
    dbgPlugins << "adding " << item->name() << " with id " << id;
    KoGenericRegistry<KisGeneratorSP>::add(id, item);
    generatorAdded(id);
}
