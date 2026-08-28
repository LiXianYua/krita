/*
 * SPDX-FileCopyrightText: 2010 Lukáš Tvrdý (lukast.dev@gmail.com)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <brushengine/kis_paintop_registry.h>


#include "kis_sketch_paintop.h"
#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>

#include "kis_global.h"

namespace { struct SketchPaintOpRegistration { SketchPaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisSketchPaintOp, KisSketchPaintOpSettings>("sketchbrush", "Sketch", KisPaintOpFactory::categoryStable(), "krita-sketch.png", PkString(), PkStringList(), 3));

}

}; }
static SketchPaintOpRegistration s_sketchPaintOpRegistration;
