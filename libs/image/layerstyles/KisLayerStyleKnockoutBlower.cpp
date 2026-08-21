/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisLayerStyleKnockoutBlower.h"

#include "kis_painter.h"
#include "KoCompositeOpRegistry.h"
#include "kis_default_bounds.h"
#include "KisImageResolutionProxy.h"


KisLayerStyleKnockoutBlower::KisLayerStyleKnockoutBlower()
{
}

KisLayerStyleKnockoutBlower::KisLayerStyleKnockoutBlower(const KisLayerStyleKnockoutBlower &rhs)
    : m_knockoutSelection(rhs.m_knockoutSelection ? new KisSelection(*rhs.m_knockoutSelection) : nullptr)
{
}

KisSelectionSP KisLayerStyleKnockoutBlower::knockoutSelectionLazy()
{
    {
        PkReadLocker l(&m_lock);
        if (m_knockoutSelection) {
            return m_knockoutSelection;
        }
    }

    {
        PkWriteLocker l(&m_lock);
        if (m_knockoutSelection) {
            return m_knockoutSelection;
        } else {
            m_knockoutSelection = new KisSelection(new KisSelectionEmptyBounds(),
                                                   KisImageResolutionProxy::identity());
            return m_knockoutSelection;
        }
    }
}

void KisLayerStyleKnockoutBlower::setKnockoutSelection(KisSelectionSP selection)
{
    PkWriteLocker l(&m_lock);
    m_knockoutSelection = selection;
}

void KisLayerStyleKnockoutBlower::resetKnockoutSelection()
{
    PkWriteLocker l(&m_lock);
    m_knockoutSelection = 0;
}

void KisLayerStyleKnockoutBlower::apply(KisPainter *painter, KisPaintDeviceSP mergedStyle, const PkRect &rect) const
{
    PkReadLocker l(&m_lock);

    KIS_SAFE_ASSERT_RECOVER_NOOP(m_knockoutSelection);

    painter->setOpacityToUnit();
    painter->setChannelFlags(PkBitArray());
    painter->setCompositeOpId(COMPOSITE_COPY);
    painter->setSelection(m_knockoutSelection);
    painter->bitBlt(rect.topLeft(), mergedStyle, rect);
    painter->setSelection(0);
}

bool KisLayerStyleKnockoutBlower::isEmpty() const
{
    PkReadLocker l(&m_lock);
    return !m_knockoutSelection;
}
