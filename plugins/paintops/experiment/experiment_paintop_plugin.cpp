/*
 * SPDX-FileCopyrightText: 2008 Lukáš Tvrdý (lukast.dev@gmail.com)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <brushengine/kis_paintop_registry.h>

#include "kis_experiment_paintop.h"
#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>

#include "kis_global.h"

namespace { struct ExperimentPaintOpRegistration { ExperimentPaintOpRegistration()
{
    KisPaintOpRegistry *r = KisPaintOpRegistry::instance();
    r->add(new KisSimplePaintOpFactory<KisExperimentPaintOp,
           KisExperimentPaintOpSettings>("experimentbrush",
                                               "Shape",
                                               KisPaintOpFactory::categoryStable(),
                                               "krita-experiment.png",
                                               PkString(), PkStringList(), 5,
                                               false));
}

}; }
static ExperimentPaintOpRegistration s_experimentPaintOpRegistration;
