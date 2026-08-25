/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_lod_capable_layer_offset.h"

#include "kis_lod_transform.h"

namespace KisLodSwitchingWrapperDetail
{
PkPoint syncLodNValue(const PkPoint &value, int lod) {
    return {KisLodTransform::coordToLodCoord(value.x(), lod),
            KisLodTransform::coordToLodCoord(value.y(), lod)};
}
}

