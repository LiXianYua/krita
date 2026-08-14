/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SHAPE_SELECTION_MARKER_H
#define KIS_SHAPE_SELECTION_MARKER_H

#include <KoShapeUserData.h>

/** Marks a shape as belonging to a vector selection. */
class KisShapeSelectionMarker : public KoShapeUserData
{
    KoShapeUserData *clone() const override
    {
        return new KisShapeSelectionMarker(*this);
    }
};

#endif // KIS_SHAPE_SELECTION_MARKER_H
