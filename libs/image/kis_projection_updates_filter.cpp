/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_projection_updates_filter.h"


#include <QtGlobal>
#include <PkRect.h>

KisProjectionUpdatesFilter::~KisProjectionUpdatesFilter()
{
}

bool KisDropAllProjectionUpdatesFilter::filter(KisImage *image, KisNode *node, const PkVector<PkRect> &rects, KisProjectionUpdateFlags flags)
{
    Q_UNUSED(image);
    Q_UNUSED(node);
    Q_UNUSED(rects);
    Q_UNUSED(flags);
    return true;
}

bool KisDropAllProjectionUpdatesFilter::filterRefreshGraph(KisImage *image, KisNode *node, const PkVector<PkRect> &rects, const PkRect &cropRect, KisProjectionUpdateFlags flags)
{
    Q_UNUSED(image);
    Q_UNUSED(node);
    Q_UNUSED(rects);
    Q_UNUSED(cropRect);
    Q_UNUSED(flags);
    return true;
}
