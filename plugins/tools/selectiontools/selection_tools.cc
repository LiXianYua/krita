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

PkList<SelectionToolActionDescriptor> selectionToolActionDescriptors()
{
    return {
        {PkString("KisToolSelectPolygonal"), PkString("undo_polygon_selection"),
         SelectionToolAction::UndoPolygonSelection, false},
        {PkString("KisToolSelectMagnetic"), PkString("undo_polygon_selection"),
         SelectionToolAction::UndoPolygonSelection, false},
        {PkString("KisToolSelectMagnetic"),
         PkString("magnetic_continued_mode_modifier"),
         SelectionToolAction::MagneticContinuedModeModifier, true}
    };
}

bool dispatchSelectionToolAction(
    KoToolBase *tool,
    SelectionToolAction action,
    SelectionToolActionPhase phase)
{
    if (!tool) return false;

    if (action == SelectionToolAction::UndoPolygonSelection &&
        phase == SelectionToolActionPhase::Trigger) {
        if (auto *polygon = dynamic_cast<KisToolSelectPolygonal *>(tool)) {
            polygon->undoSelectionOrCancel();
            return true;
        }
        if (auto *magnetic = dynamic_cast<KisToolSelectMagnetic *>(tool)) {
            magnetic->undoPoints();
            return true;
        }
        return false;
    }

    if (action == SelectionToolAction::MagneticContinuedModeModifier) {
        auto *magnetic = dynamic_cast<KisToolSelectMagnetic *>(tool);
        if (!magnetic || phase == SelectionToolActionPhase::Trigger) return false;
        magnetic->setContinuedModeModifierPressed(
            phase == SelectionToolActionPhase::Press);
        return true;
    }

    return false;
}
