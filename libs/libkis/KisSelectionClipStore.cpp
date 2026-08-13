/*
 * SPDX-FileCopyrightText: 2026 OpenAI
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisSelectionClipStore.h"

#include <kis_paint_device.h>

KisSelectionClipStore *KisSelectionClipStore::instance()
{
    static KisSelectionClipStore store;
    return &store;
}

void KisSelectionClipStore::setClip(KisPaintDeviceSP clip, const QPoint &topLeft)
{
    m_clip = clip;
    m_topLeft = topLeft;
}

KisPaintDeviceSP KisSelectionClipStore::clip() const
{
    return m_clip;
}

QPoint KisSelectionClipStore::topLeft() const
{
    return m_topLeft;
}

bool KisSelectionClipStore::hasClip() const
{
    return bool(m_clip);
}

void KisSelectionClipStore::clear()
{
    m_clip.clear();
    m_topLeft = QPoint();
}
