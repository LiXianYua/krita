/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisMoveBoundsCalculationJob.h"
#include "kis_node.h"
#include "kis_selection.h"
#include "kis_layer_utils.h"
#include "kis_basic_tools_string_utils.h"
#include <cstdint>

KisMoveBoundsCalculationJob::KisMoveBoundsCalculationJob(KisNodeList nodes,
                                                         KisSelectionSP selection,
                                                         PkObject *requestedBy)
    : m_nodes(nodes),
      m_selection(selection),
      m_requestedBy(requestedBy)
{
    setExclusive(true);
}

void KisMoveBoundsCalculationJob::run()
{
    PkRect handlesRect;

    for (const KisNodeSP &node : m_nodes) {
        handlesRect |= KisLayerUtils::recursiveTightNodeVisibleBounds(node);
    }

    if (m_selection) {
        handlesRect &= m_selection->selectedExactRect();
    }

    sigCalculationFinished(handlesRect);
}

void KisMoveBoundsCalculationJob::sigCalculationFinished(const PkRect &bounds)
{
    PkObject::activateSignal<const PkRect &>(
        this,
        PkMemberFnKey::from(&KisMoveBoundsCalculationJob::sigCalculationFinished),
        bounds);
}

bool KisMoveBoundsCalculationJob::overrides(const KisSpontaneousJob *_otherJob)
{
    const KisMoveBoundsCalculationJob *otherJob =
        dynamic_cast<const KisMoveBoundsCalculationJob*>(_otherJob);

    return otherJob && otherJob->m_requestedBy == m_requestedBy;
}

int KisMoveBoundsCalculationJob::levelOfDetail() const
{
    return 0;
}

PkString KisMoveBoundsCalculationJob::debugName() const
{
    return PkString("KisMoveBoundsCalculationJob requestedBy=%1 nodes=%2")
        .arg(KisBasicToolsString::number(reinterpret_cast<std::uintptr_t>(m_requestedBy)))
        .arg(KisBasicToolsString::number(m_nodes.size()));
}
