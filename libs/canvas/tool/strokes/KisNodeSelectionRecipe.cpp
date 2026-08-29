/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisNodeSelectionRecipe.h"

#include "kis_layer_utils.h"
#include "kis_lod_transform.h"
#include "kis_node.h"
#include "kis_layer.h"
#include "kis_group_layer.h"
#include <KoColor.h>

namespace {

KisNodeSP findNode(KisNodeSP node, const QPoint &point, bool wholeGroup, bool editableOnly = true)
{
    KisNodeSP foundNode = 0;
    while (node) {
        KisLayerSP layer = dynamic_cast<KisLayer*>(node.data());

        if (!layer || !layer->isEditable()) {
            node = node->prevSibling();
            continue;
        }

        KoColor color(layer->projection()->colorSpace());
        layer->projection()->pixel(point.x(), point.y(), &color);

        KisGroupLayerSP group = dynamic_cast<KisGroupLayer*>(layer.data());

        if ((group && group->passThroughMode()) ||
            color.opacityU8() != OPACITY_TRANSPARENT_U8) {
            if (layer->inherits("KisGroupLayer") && (!editableOnly || layer->isEditable())) {
                foundNode = findNode(node->lastChild(), point, wholeGroup, editableOnly);
            } else {
                foundNode = !wholeGroup ? node : node->parent();
            }
        }

        if (foundNode) break;
        node = node->prevSibling();
    }

    return foundNode;
}

}

KisNodeSelectionRecipe::KisNodeSelectionRecipe(KisNodeList _selectedNodes)
    : selectedNodes(_selectedNodes),
      mode(SelectedLayer)
{
}

KisNodeSelectionRecipe::KisNodeSelectionRecipe(KisNodeList _selectedNodes, KisNodeSelectionRecipe::SelectionMode _mode, QPoint _pickPoint)
    : selectedNodes(_selectedNodes),
      mode(_mode),
      pickPoint(_pickPoint)
{
}

KisNodeSelectionRecipe::KisNodeSelectionRecipe(const KisNodeSelectionRecipe &rhs, int levelOfDetail)
    : KisNodeSelectionRecipe(rhs)
{
    KisLodTransform t(levelOfDetail);
    pickPoint = t.map(rhs.pickPoint);
}

KisNodeList KisNodeSelectionRecipe::selectNodesToProcess() const
{
    if (selectedNodes.isEmpty() || mode == SelectedLayer) {
        return selectedNodes;
    }

    KisNodeSP activeRoot = KisLayerUtils::findIsolationRoot(selectedNodes.first());
    const bool wholeGroup = mode == Group;

    KisNodeList result;

    if (!activeRoot) {
        activeRoot = KisLayerUtils::findRoot(selectedNodes.first());
    }

    KisNodeSP node = findNode(activeRoot, pickPoint, wholeGroup);
    if (node) {
        result = {node};
    }

    return result;
}
