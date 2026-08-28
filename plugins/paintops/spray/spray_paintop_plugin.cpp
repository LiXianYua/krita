#include <PkString.h>
#include <brushengine/kis_paintop_registry.h>
#include "kis_spray_paintop.h"
#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>

namespace { struct KisSprayPaintOpRegistration { KisSprayPaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisSprayPaintOp, KisSprayPaintOpSettings>("spraybrush", "Spray", KisPaintOpFactory::categoryStable(), "krita-spray.png", PkString(), PkStringList(), 6));
}
}; }
static KisSprayPaintOpRegistration s_sprayPaintOpRegistration;
