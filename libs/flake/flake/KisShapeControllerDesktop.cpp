/*
 * SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2026 Krita contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisShapeControllerDesktop.h"

#include <KoCanvasController.h>
#include <KoSelection.h>
#include <KoShapeManager.h>
#include <KoToolManager.h>
#include <KoSelectedShapesProxy.h>

#include "canvas/kis_canvas2.h"
#include "kis_node_shape.h"
#include "kis_selection.h"
#include "kis_shape_controller.h"
#include "kis_shape_controller_ui_adapter.h"
#include "kis_shape_layer.h"
#include "KisViewManager.h"

namespace
{
class KisShapeControllerDesktopAdapter final : public KisShapeControllerUiAdapter
{
public:
    KisSelectionSP imageSelection() const override
    {
        KisCanvas2 *canvas = activeCanvas();
        return canvas ? canvas->viewManager()->selection() : KisSelectionSP();
    }

    KoShapeLayer *activeShapeLayer() const override
    {
        KoSelection *selection = activeShapeSelection();
        return selection ? selection->activeLayer() : nullptr;
    }

    void nodeShapeAboutToBeDestroyed(KisNodeShape *shape) override
    {
        KoSelection *selection = activeShapeSelection();
        if (selection && selection->activeLayer() == shape) {
            selection->setActiveLayer(nullptr);
        }
    }

    void nodeShapeEditabilityChanged(KisNodeShape *shape) override
    {
        KoSelection *selection = activeShapeSelection();
        KoShapeLayer *activeLayer = selection ? selection->activeLayer() : nullptr;
        if (!activeLayer) {
            return;
        }

        bool isDescendant = false;
        for (KoShapeLayer *layer = activeLayer; layer && !isDescendant;
             layer = dynamic_cast<KoShapeLayer *>(layer->parent())) {
            isDescendant = layer == shape;
        }

        KisShapeLayer *shapeLayer = dynamic_cast<KisShapeLayer *>(shape->node().data());
        if (isDescendant || (shapeLayer && shapeLayer == activeLayer)) {
            selection->setActiveLayer(activeLayer);
        }
    }

private:
    static KisCanvas2 *activeCanvas()
    {
        KoCanvasController *controller = KoToolManager::instance()->activeCanvasController();
        return controller ? dynamic_cast<KisCanvas2 *>(controller->canvas()) : nullptr;
    }

    static KoSelection *activeShapeSelection()
    {
        KisCanvas2 *canvas = activeCanvas();
        return canvas ? canvas->selectedShapesProxy()->selection() : nullptr;
    }
};

KisShapeControllerDesktopAdapter s_desktopAdapter;
}

void initializeKisShapeControllerDesktopServices()
{
    KisShapeControllerUiAdapter::setInstance(&s_desktopAdapter);
}

void clearKisShapeControllerDesktopServices()
{
    if (KisShapeControllerUiAdapter::instance() == &s_desktopAdapter) {
        KisShapeControllerUiAdapter::setInstance(nullptr);
    }
}

void setInitialShapeForCanvas(KisShapeController *controller, KisCanvas2 *canvas)
{
    KisImageSP image = controller->currentImage();
    if (!image) {
        return;
    }

    KisNodeSP rootNode = image->root();
    KoShapeLayer *rootShape = controller->shapeForNode(rootNode);
    if (rootShape) {
        Q_ASSERT(canvas);
        Q_ASSERT(canvas->shapeManager());
        KoSelection *selection = canvas->shapeManager()->selection();
        if (selection) {
            selection->select(rootShape);
            KoToolManager::instance()->switchToolRequested(
                KoToolManager::instance()->preferredToolForSelection(selection->selectedShapes()));
        }
    }
}
