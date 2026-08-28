/*
 * SPDX-FileCopyrightText: 2010 Lukáš Tvrdý (lukast.dev@gmail.com)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <brushengine/kis_paintop_registry.h>


#include "kis_particle_paintop.h"

#include <kis_simple_paintop_factory.h>
#include <kis_image.h>
#include <kis_node.h>

#include "kis_global.h"

namespace { struct ParticlePaintOpRegistration { ParticlePaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisParticlePaintOp, KisParticlePaintOpSettings>("particlebrush", "Particle", KisPaintOpFactory::categoryStable(), "krita-particle.png", PkString(), PkStringList(), 11, false));
}

}; }
static ParticlePaintOpRegistration s_particlePaintOpRegistration;
