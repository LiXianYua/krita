/*
 * defaultpaintops_plugin.cc -- Part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Boudewijn Rempt (boud@valdyas.org)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "defaultpaintops_plugin.h"
#include <KoCompositeOpRegistry.h>

#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>
#include "kis_brushop.h"
#include "kis_duplicateop.h"
#include "kis_duplicateop_settings.h"
#include "kis_global.h"
#include <brushengine/kis_paintop_registry.h>
#include "KisBrushOpSettings.h"

namespace {
struct DefaultPaintOpsRegistration
{
    DefaultPaintOpsRegistration()
    {
        KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
        r->add(new KisSimplePaintOpFactory<KisBrushOp, KisBrushOpSettings>("paintbrush", "Pixel", KisPaintOpFactory::categoryStable(), "krita-paintbrush.png", PkString(), PkStringList(), 1));
        r->add(new KisSimplePaintOpFactory<KisDuplicateOp, KisDuplicateOpSettings>("duplicate", "Clone", KisPaintOpFactory::categoryStable(), "krita-duplicate.png", PkString(), PkStringList{COMPOSITE_COPY}, 15));
    }
};
}

static DefaultPaintOpsRegistration s_defaultPaintOpsRegistration;
