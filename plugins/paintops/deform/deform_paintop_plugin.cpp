/*
 * SPDX-FileCopyrightText: 2008 Lukáš Tvrdý (lukast.dev@gmail.com)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoCompositeOpRegistry.h>

#include <brushengine/kis_paintop_registry.h>

#include "kis_deform_paintop.h"
#include "kis_global.h"
#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>

namespace { struct DeformPaintOpRegistration { DeformPaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisDeformPaintOp, KisDeformPaintOpSettings>("deformbrush", "Deform", KisPaintOpFactory::categoryStable(), "krita-deform.png", PkString(), PkStringList(COMPOSITE_COPY), 16));
}

}; }
static DeformPaintOpRegistration s_deformPaintOpRegistration;
