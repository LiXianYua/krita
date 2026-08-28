#include <PkString.h>
/*
 * hairy_paintop_plugin.cc -- Part of Krita
 *
 * SPDX-FileCopyrightText: 2008 Lukáš Tvrdý (lukast.dev@gmail.com)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <brushengine/kis_paintop_registry.h>
#include "kis_simple_paintop_factory.h"
#include "kis_hairy_paintop.h"
#include "kis_hairy_paintop_settings.h"
#include <kis_image.h>
#include <kis_node.h>

namespace { struct HairyPaintOpRegistration { HairyPaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisHairyPaintOp, KisHairyPaintOpSettings>("hairybrush", "Bristle", KisPaintOpFactory::categoryStable(), "krita-sumi.png", PkString(), PkStringList(), 4));
}
}; }
static HairyPaintOpRegistration s_hairyPaintOpRegistration;
