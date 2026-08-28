/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "assistant_tool.h"

#include "ConcentricEllipseAssistant.h"
#include "CurvilinearPerspectiveAssistant.h"
#include "EllipseAssistant.h"
#include "FisheyePointAssistant.h"
#include "InfiniteRulerAssistant.h"
#include "ParallelRulerAssistant.h"
#include "PerspectiveAssistant.h"
#include "PerspectiveEllipseAssistant.h"
#include "RulerAssistant.h"
#include "SplineAssistant.h"
#include "TwoPointAssistant.h"
#include "VanishingPointAssistant.h"

#include <PkString.h>

#include <mutex>

PkString assistantToolPluginId()
{
    return PkString("AssistantTool");
}

void registerAssistantFactories()
{
    static std::once_flag once;
    std::call_once(once, [] {
        KisPaintingAssistantFactoryRegistry *registry = KisPaintingAssistantFactoryRegistry::instance();
        registry->add(new RulerAssistantFactory);
        registry->add(new EllipseAssistantFactory);
        registry->add(new SplineAssistantFactory);
        registry->add(new PerspectiveAssistantFactory);
        registry->add(new VanishingPointAssistantFactory);
        registry->add(new InfiniteRulerAssistantFactory);
        registry->add(new ParallelRulerAssistantFactory);
        registry->add(new ConcentricEllipseAssistantFactory);
        registry->add(new FisheyePointAssistantFactory);
        registry->add(new TwoPointAssistantFactory);
        registry->add(new PerspectiveEllipseAssistantFactory);
        registry->add(new CurvilinearPerspectiveAssistantFactory);
    });
}

namespace
{
struct AssistantFactoryRegistration
{
    AssistantFactoryRegistration() { registerAssistantFactories(); }
};

const AssistantFactoryRegistration registration;
}
