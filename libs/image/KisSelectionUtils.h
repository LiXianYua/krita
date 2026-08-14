/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SELECTION_UTILS_H
#define KIS_SELECTION_UTILS_H

#include <kritaimage_export.h>
#include <kis_types.h>

namespace KisSelectionUtils
{

/**
 * Returns the local selection belonging to the active layer, falling back to
 * the image-global selection. When the active node is a mask, its parent layer
 * is used, matching the document-view selection semantics.
 */
KRITAIMAGE_EXPORT KisSelectionSP activeSelectionForNode(KisImageSP image, KisNodeSP node);

/**
 * Returns whether the active layer's local selection mask is editable. Global
 * selections and layers without a local selection are always editable.
 */
KRITAIMAGE_EXPORT bool isSelectionEditable(KisNodeSP node);

}

#endif // KIS_SELECTION_UTILS_H
