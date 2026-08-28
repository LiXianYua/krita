/*
 * curvepaintop_plugin.cc -- Part of Krita
 *
 * SPDX-FileCopyrightText: 2008 Lukáš Tvrdý (lukast.dev@gmail.com)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <brushengine/kis_paintop_registry.h>

#include "kis_curve_paintop_settings.h"
#include "kis_curve_paintop.h"
#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>
#include "kis_global.h"

namespace { struct CurvePaintOpRegistration { CurvePaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisCurvePaintOp, KisCurvePaintOpSettings>("curvebrush", "Curve", KisPaintOpFactory::categoryStable(), "krita-curve.png", PkString(), PkStringList(), 9));

}

}; }
static CurvePaintOpRegistration s_curvePaintOpRegistration;
