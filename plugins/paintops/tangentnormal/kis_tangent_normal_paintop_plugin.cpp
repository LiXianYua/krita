/*
 *  SPDX-FileCopyrightText: 2015 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkString.h>
#include <brushengine/kis_paintop_registry.h>
#include "kis_tangent_normal_paintop.h"
#include <kis_brush_based_paintop_settings.h>
#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>

namespace { struct KisTangentNormalPaintOpRegistration { KisTangentNormalPaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisTangentNormalPaintOp, KisBrushBasedPaintOpSettings>("tangentnormal", "Tangent Normal", KisPaintOpFactory::categoryStable(), "krita-tangentnormal.png", PkString(), PkStringList(), 16));
}
}; }
static KisTangentNormalPaintOpRegistration s_tangentnormalPaintOpRegistration;
