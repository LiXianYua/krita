/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisSelectionUtils.h"

#include "kis_image.h"
#include "kis_layer.h"
#include "kis_mask.h"
#include "kis_selection.h"
#include "kis_selection_mask.h"

namespace
{

KisLayerSP activeLayerForNode(KisNodeSP node)
{
    if (dynamic_cast<KisMask *>(node.data())) {
        node = node->parent();
    }
    return dynamic_cast<KisLayer *>(node.data());
}

}

namespace KisSelectionUtils
{

KisSelectionSP activeSelectionForNode(KisImageSP image, KisNodeSP node)
{
    KisLayerSP layer = activeLayerForNode(node);
    if (layer) {
        return layer->selection();
    }
    return image ? image->globalSelection() : KisSelectionSP();
}

bool isSelectionEditable(KisNodeSP node)
{
    KisLayerSP layer = activeLayerForNode(node);
    if (layer) {
        KisSelectionMaskSP mask = layer->selectionMask();
        if (mask) {
            return mask->isEditable();
        }
    }
    return true;
}

}
