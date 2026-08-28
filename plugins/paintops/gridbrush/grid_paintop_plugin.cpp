/*
 * SPDX-FileCopyrightText: 2009 Lukáš Tvrdý (lukast.dev@gmail.com)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <brushengine/kis_paintop_registry.h>
#include <kis_image.h>
#include <kis_node.h>

#include "kis_grid_paintop.h"
#include "kis_simple_paintop_factory.h"

#include "kis_global.h"

namespace { struct GridPaintOpRegistration { GridPaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisGridPaintOp, KisGridPaintOpSettings>("gridbrush", "Grid",
                                                                                                             KisPaintOpFactory::categoryStable(), "krita-grid.png", PkString(), PkStringList(), 8));

}

}; }
static GridPaintOpRegistration s_gridPaintOpRegistration;
