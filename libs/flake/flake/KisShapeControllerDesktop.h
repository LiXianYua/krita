/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SHAPE_CONTROLLER_DESKTOP_H
#define KIS_SHAPE_CONTROLLER_DESKTOP_H

#include "kritaui_export.h"

class KisCanvas2;
class KisShapeController;

KRITAUI_EXPORT void initializeKisShapeControllerDesktopServices();
KRITAUI_EXPORT void clearKisShapeControllerDesktopServices();
KRITAUI_EXPORT void setInitialShapeForCanvas(KisShapeController *controller,
                                             KisCanvas2 *canvas);

#endif // KIS_SHAPE_CONTROLLER_DESKTOP_H
