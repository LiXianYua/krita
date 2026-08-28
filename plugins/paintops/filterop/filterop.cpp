/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoCompositeOpRegistry.h>

#include <brushengine/kis_paintop_registry.h>
#include <kis_image.h>
#include <kis_node.h>
#include "kis_simple_paintop_factory.h"
#include "kis_filterop.h"
#include "kis_filterop_settings.h"

namespace { struct FilterOpRegistration { FilterOpRegistration()
{
    PkStringList whiteList;
    whiteList << COMPOSITE_COPY;

    // This is not a gui plugin; only load it when the doc is created.
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisFilterOp, KisFilterOpSettings>("filter", "Filter", KisPaintOpFactory::categoryStable(), "krita-filterop.png", PkString(), whiteList, 17));

}

}; }
static FilterOpRegistration s_filterOpRegistration;
