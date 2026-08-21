/*
 *  SPDX-FileCopyrightText: 2003 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2004-2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "filter/kis_filter_registry.h"

#include <math.h>

#include "kis_debug.h"
#include "kis_types.h"

#include "kis_paint_device.h"
#include "filter/kis_filter.h"
#include "kis_filter_configuration.h"

KisFilterRegistry::KisFilterRegistry()
    : PkObject()
{
}

KisFilterRegistry::~KisFilterRegistry()
{
    dbgRegistry << "deleting KisFilterRegistry";
    for (KisFilterSP filter : values()) {
        remove(filter->id());
        filter.clear();
    }
}

KisFilterRegistry* KisFilterRegistry::instance()
{
    static KisFilterRegistry s_instance;
    return &s_instance;
}

void KisFilterRegistry::add(KisFilterSP item)
{
    add(item->id(), item);
}

void KisFilterRegistry::add(const PkString &id, KisFilterSP item)
{
    KoGenericRegistry<KisFilterSP>::add(id, item);
    filterAdded(id);
}

KisFilterSP KisFilterRegistry::fallbackFilter() const
{
    return value("gaussian blur");
}


