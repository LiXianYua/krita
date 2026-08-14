/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_shape_controller.h"


#include <klocalizedstring.h>

#include <KoShape.h>
#include <KoShapeContainer.h>
#include <KoShapeManager.h>
#include <KoShapeLayer.h>
#include <KoColorSpaceConstants.h>

#include "kis_shape_selection.h"
#include "kis_selection.h"
#include "kis_selection_mask.h"
#include "kis_selection_component.h"
#include "kis_image.h"
#include "kis_group_layer.h"
#include "kis_node_shape.h"
#include "kis_node_shapes_graph.h"
#include "kis_name_server.h"
#include "kis_shape_layer.h"
#include "kis_node.h"
#include "kis_shape_controller_ui_adapter.h"

#include <KoDocumentResourceManager.h>
#include <commands/kis_image_layer_add_command.h>
#include "kis_signal_auto_connection.h"
#include "kis_command_utils.h"


struct KisShapeController::Private
{
public:
    KisNameServer *nameServer;
    KisSignalAutoConnectionsStore imageConnections;

    KisNodeShapesGraph shapesGraph;
};

KisShapeController::KisShapeController(KisNameServer *nameServer, KUndo2Stack *undoStack, QObject *parent)
    : KisDummiesFacadeBase(parent)
    , m_d(new Private())
{
    m_d->nameServer = nameServer;
    resourceManager()->setUndoStack(undoStack);
}


KisShapeController::~KisShapeController()
{
    KisNodeDummy *node = m_d->shapesGraph.rootDummy();
    if (node) {
        m_d->shapesGraph.removeNode(node->node());
    }

    delete m_d;
}

void KisShapeController::slotUpdateDocumentResolution()
{
    KisImageSP image = this->image();

    if (image) {
        const qreal pixelsPerInch = image->xRes() * 72.0;
        resourceManager()->setResource(KoDocumentResourceManager::DocumentResolution, pixelsPerInch);
    }
}

void KisShapeController::slotUpdateDocumentSize()
{
    KisImageSP image = this->image();

    if (image) {
        resourceManager()->setResource(KoDocumentResourceManager::DocumentRectInPixels, image->bounds());
    }
}

void KisShapeController::addNodeImpl(KisNodeSP node, KisNodeSP parent, KisNodeSP aboveThis)
{
    KisNodeShape *newShape =
        m_d->shapesGraph.addNode(node, parent, aboveThis);
    // XXX: what are we going to do with this shape?
    Q_UNUSED(newShape);

    KisShapeLayer *shapeLayer = dynamic_cast<KisShapeLayer*>(node.data());
    if (shapeLayer) {
        // Forward local shape-manager signals through the stable controller.
        connect(shapeLayer, SIGNAL(selectionChanged()),
                SIGNAL(selectionChanged()));
        connect(shapeLayer->shapeManager(), SIGNAL(selectionContentChanged()),
                SIGNAL(selectionContentChanged()));
        connect(shapeLayer, SIGNAL(currentLayerChanged(const KoShapeLayer*)),
                SIGNAL(currentLayerChanged(const KoShapeLayer*)));
    }
}

void KisShapeController::removeNodeImpl(KisNodeSP node)
{
    KisShapeLayer *shapeLayer = dynamic_cast<KisShapeLayer*>(node.data());
    if (shapeLayer) {
        shapeLayer->disconnect(this);
    }

    m_d->shapesGraph.removeNode(node);
}

bool KisShapeController::hasDummyForNode(KisNodeSP node) const
{
    return m_d->shapesGraph.containsNode(node);
}

KisNodeDummy* KisShapeController::dummyForNode(KisNodeSP node) const
{
    return m_d->shapesGraph.nodeToDummy(node);
}

KisNodeDummy* KisShapeController::rootDummy() const
{
    return m_d->shapesGraph.rootDummy();
}

int KisShapeController::dummiesCount() const
{
    return m_d->shapesGraph.shapesCount();
}
static inline bool belongsToShapeSelection(KoShape* shape) {
    return dynamic_cast<KisShapeSelectionMarker*>(shape->userData());
}

KoShapeContainer *KisShapeController::createParentForShapes(const QList<KoShape *> shapes, bool forceNewLayer, KUndo2Command *parentCommand)
{
    KoShapeContainer *resultParent = 0;
    KisCommandUtils::CompositeCommand *resultCommand =
        new KisCommandUtils::CompositeCommand(parentCommand);

    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!shapes.isEmpty(), resultParent);
    Q_FOREACH (KoShape *shape, shapes) {
        KIS_SAFE_ASSERT_RECOVER_BREAK(!shape->parent());
    }

    KisShapeControllerUiAdapter *adapter = KisShapeControllerUiAdapter::instance();

    const bool baseBelongsToSelection = belongsToShapeSelection(shapes.first());
    bool allSameBelongsToShapeSelection = true;

    Q_FOREACH (KoShape *shape, shapes) {
        allSameBelongsToShapeSelection &= belongsToShapeSelection(shape) == baseBelongsToSelection;
    }

    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(!baseBelongsToSelection || allSameBelongsToShapeSelection, resultParent);

    if (baseBelongsToSelection && allSameBelongsToShapeSelection) {
        KisSelectionSP selection = adapter ? adapter->imageSelection() : image()->globalSelection();
        if (selection) {
            KisSelectionComponent* shapeSelectionComponent = selection->shapeSelection();

            if (!shapeSelectionComponent) {
                shapeSelectionComponent = new KisShapeSelection(this, selection);
                resultCommand->addCommand(selection->convertToVectorSelection(shapeSelectionComponent));
            }

            KisShapeSelection * shapeSelection = static_cast<KisShapeSelection*>(shapeSelectionComponent);
            resultParent = shapeSelection;
        }
    } else {
        KisShapeLayer *shapeLayer =
                dynamic_cast<KisShapeLayer*>(
                    adapter ? adapter->activeShapeLayer() : nullptr);

        if (!shapeLayer || forceNewLayer) {
            shapeLayer = new KisShapeLayer(this, image(),
                                           i18n("Vector Layer %1", m_d->nameServer->number()),
                                           OPACITY_OPAQUE_U8);

            resultCommand->addCommand(
                        new KisImageLayerAddCommand(image(),
                                                    shapeLayer,
                                                    image()->rootLayer(),
                                                    image()->rootLayer()->childCount()));
        }

        resultParent = shapeLayer;
    }

    return resultParent;
}

QRectF KisShapeController::documentRectInPixels() const
{
    KisImageSP image = this->image();
    return image ? image->bounds() : QRect(0, 0, 666, 777);
}

qreal KisShapeController::pixelsPerInch() const
{
    KisImageSP image = this->image();
    return image ? image->xRes() * 72.0 : 72.0;
}

void KisShapeController::setImage(KisImageWSP image, KisNodeSP activeNode)
{
    m_d->imageConnections.clear();

    if (image) {
        m_d->imageConnections.addConnection(image, SIGNAL(sigResolutionChanged(double, double)), this, SLOT(slotUpdateDocumentResolution()));
        m_d->imageConnections.addConnection(image, SIGNAL(sigSizeChanged(QPointF, QPointF)), this, SLOT(slotUpdateDocumentSize()));
    }

    KisDummiesFacadeBase::setImage(image, activeNode);

    slotUpdateDocumentResolution();
    slotUpdateDocumentSize();
}

KoShapeLayer* KisShapeController::shapeForNode(KisNodeSP node) const
{
    if (node) {
        return m_d->shapesGraph.nodeToShape(node);
    }
    return 0;
}

KoShapeManager *KisShapeController::shapeManagerForNode(KisNodeSP node) const
{
    KoShapeManager *shapeManager = nullptr;
    KisSelectionSP selection;

    if (auto *shapeLayer = dynamic_cast<KisShapeLayer *>(node.data())) {
        shapeManager = shapeLayer->shapeManager();
    } else if (auto *mask = dynamic_cast<KisSelectionMask *>(node.data())) {
        selection = mask->selection();
    }

    if (!shapeManager && selection && selection->hasShapeSelection()) {
        auto *shapeSelection =
            dynamic_cast<KisShapeSelection *>(selection->shapeSelection());
        KIS_ASSERT_RECOVER_RETURN_VALUE(shapeSelection, nullptr);
        shapeManager = shapeSelection->shapeManager();
    }

    return shapeManager;
}

KisImageSP KisShapeController::currentImage() const
{
    return image();
}
