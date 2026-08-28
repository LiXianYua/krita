#include <PkString.h>
#include <brushengine/kis_paintop_registry.h>
#include "kis_hatching_paintop.h"
#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>

namespace { struct KisHatchingPaintOpRegistration { KisHatchingPaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisHatchingPaintOp, KisHatchingPaintOpSettings>("hatchingbrush", "Hatching", KisPaintOpFactory::categoryStable(), "krita-hatching.png", PkString(), PkStringList(), 7));
}
}; }
static KisHatchingPaintOpRegistration s_hatchingPaintOpRegistration;
