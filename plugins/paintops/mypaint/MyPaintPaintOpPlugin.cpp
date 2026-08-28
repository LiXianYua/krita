/*
 * SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MyPaintPaintOpPlugin.h"

#include <KisResourceLoader.h>
#include <KisResourceLoaderRegistry.h>
#include <brushengine/kis_paintop_registry.h>
#include "MyPaintPaintOpFactory.h"
#include "MyPaintPaintOpPreset.h"

namespace
{
struct MyPaintOpRegistration
{
    MyPaintOpRegistration()
    {
        KisResourceLoaderRegistry::instance()->registerLoader(
            new KisResourceLoader<KisMyPaintPaintOpPreset>(
                ResourceSubType::MyPaintPaintOpPresets,
                ResourceType::PaintOpPresets,
                "MyPaint Brush Presets",
                PkStringList() << "application/x-mypaint-brush"));
        KisPaintOpRegistry::instance()->add(new KisMyPaintOpFactory());
    }
};
}

static MyPaintOpRegistration s_myPaintOpRegistration;
