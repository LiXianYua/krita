/*
 * tool_transform.cc -- Part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Boudewijn Rempt (boud@valdyas.org)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tool_transform.h"

#include <kis_global.h>
#include <kis_types.h>
#include <KoToolRegistry.h>


#include "kis_tool_transform.h"
#include "kis_transform_mask_params_factory_registry.h"
#include "kis_transform_mask_adapter.h"
#include "KisAnimatedTransformMaskParamsHolder.h"

namespace {

KisAnimatedTransformParamsHolderInterfaceSP createAnimatedParamsHolder(KisDefaultBoundsBaseSP defaultBounds)
{
    return toQShared(new KisAnimatedTransformMaskParamsHolder(defaultBounds));
}

} // namespace



void registerToolTransformPlugin()
{
    registerToolTransformPlugin([](TransformToolRegistrationStep step) {
        switch (step) {
        case TransformToolRegistrationStep::ToolFactory:
            KoToolRegistry::instance()->add(new KisToolTransformFactory());
            break;
        case TransformToolRegistrationStep::AnimatedParamsHolderFactory:
            KisTransformMaskParamsFactoryRegistry::instance()->setAnimatedParamsHolderFactory(&createAnimatedParamsHolder);
            break;
        case TransformToolRegistrationStep::TransformMaskFactory:
            KisTransformMaskParamsFactoryRegistry::instance()->addFactory("tooltransformparams", &KisTransformMaskAdapter::fromXML);
            break;
        case TransformToolRegistrationStep::DumbTransformMaskFactory:
            KisTransformMaskParamsFactoryRegistry::instance()->addFactory("dumbparams", &KisTransformMaskAdapter::fromDumbXML);
            break;
        }
    });
}
