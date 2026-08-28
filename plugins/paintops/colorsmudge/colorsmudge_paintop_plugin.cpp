/*
 *  SPDX-FileCopyrightText: 2011 Silvio Heinrich <plassy@web.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <brushengine/kis_paintop_registry.h>
#include "kis_colorsmudgeop_settings.h"

#include "kis_colorsmudgeop.h"
#include "kis_simple_paintop_factory.h"
#include <kis_image.h>
#include <kis_node.h>

#include "kis_global.h"

namespace
{
struct ColorSmudgePaintOpRegistration
{
    ColorSmudgePaintOpRegistration()
    {
        KisPaintOpRegistry::instance()->add(
            new KisSimplePaintOpFactory<KisColorSmudgeOp, KisColorSmudgeOpSettings>(
                "colorsmudge", "Color Smudge", KisPaintOpFactory::categoryStable(),
                "krita-colorsmudge.png", PkString(), PkStringList(), 2));
    }
};
}

static ColorSmudgePaintOpRegistration s_colorSmudgePaintOpRegistration;
