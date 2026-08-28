/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <brushengine/kis_paintop_registry.h>
#include "kis_roundmarkerop_settings.h"

#include "kis_roundmarkerop.h"
#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>

#include "kis_global.h"

namespace { struct RoundMarkerPaintOpRegistration { RoundMarkerPaintOpRegistration()
{
    KisPaintOpRegistry::instance()->add(new KisSimplePaintOpFactory<KisRoundMarkerOp, KisRoundMarkerOpSettings>(
                                            "roundmarker", "Quick Brush", KisPaintOpFactory::categoryStable(), "krita_roundmarkerop.svg",
                                            PkString(), PkStringList(), 3)
                                       );
}

}; }
static RoundMarkerPaintOpRegistration s_roundMarkerPaintOpRegistration;
