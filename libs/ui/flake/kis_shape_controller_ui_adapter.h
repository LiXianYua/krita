/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SHAPE_CONTROLLER_UI_ADAPTER_H
#define KIS_SHAPE_CONTROLLER_UI_ADAPTER_H

#include "kis_types.h"
#include "kritashapemodel_export.h"

class KisNodeShape;
class KoShapeLayer;

/**
 * The desktop view state used by the shape-domain controller.
 *
 * The document shape graph and resource manager remain usable without an
 * adapter.  Krita's desktop shell installs this port to preserve behavior
 * that intentionally follows the currently active canvas and selection.
 */
class KRITASHAPEMODEL_EXPORT KisShapeControllerUiAdapter
{
public:
    virtual ~KisShapeControllerUiAdapter();

    virtual KisSelectionSP imageSelection() const = 0;
    virtual KoShapeLayer *activeShapeLayer() const = 0;

    virtual void nodeShapeAboutToBeDestroyed(KisNodeShape *shape) = 0;
    virtual void nodeShapeEditabilityChanged(KisNodeShape *shape) = 0;

    static KisShapeControllerUiAdapter *instance();
    static void setInstance(KisShapeControllerUiAdapter *adapter);
};

#endif // KIS_SHAPE_CONTROLLER_UI_ADAPTER_H
