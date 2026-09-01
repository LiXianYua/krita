/*
 * selection_tools.cc -- Part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Boudewijn Rempt (boud@valdyas.org)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "selection_tools.h"
#include <mutex>

#include "KoToolRegistry.h"

#include "kis_global.h"
#include "kis_types.h"

#include "kis_tool_select_outline.h"
#include "kis_tool_select_polygonal.h"
#include "kis_tool_select_rectangular.h"
#include "kis_tool_select_contiguous.h"
#include "kis_tool_select_elliptical.h"
#include "kis_tool_select_path.h"
#include "kis_tool_select_similar.h"
#include "KisToolSelectMagnetic.h"

void registerSelectionTools()
{
    static std::once_flag once;
    std::call_once(once, [] {
        KoToolRegistry::instance()->add(new KisToolSelectOutlineFactory());
        KoToolRegistry::instance()->add(new KisToolSelectPolygonalFactory());
        KoToolRegistry::instance()->add(new KisToolSelectRectangularFactory());
        KoToolRegistry::instance()->add(new KisToolSelectEllipticalFactory());
        KoToolRegistry::instance()->add(new KisToolSelectContiguousFactory());
        KoToolRegistry::instance()->add(new KisToolSelectPathFactory());
        KoToolRegistry::instance()->add(new KisToolSelectSimilarFactory());
        KoToolRegistry::instance()->add(new KisToolSelectMagneticFactory());
    });
}
