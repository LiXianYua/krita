/*
 *  SPDX-FileCopyrightText: 2019 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISMOVEBOUNDSCALCULATIONJOB_H
#define KISMOVEBOUNDSCALCULATIONJOB_H

#include <PkObject.h>
#include <PkRect.h>
#include <PkString.h>
#include "kis_spontaneous_job.h"
#include "kis_types.h"
#include "kis_selection.h"

class KisMoveBoundsCalculationJob : public PkObject, public KisSpontaneousJob
{
public:
    KisMoveBoundsCalculationJob(KisNodeList nodes, KisSelectionSP selection, PkObject *requestedBy);

    void run() override;
    bool overrides(const KisSpontaneousJob *otherJob) override;
    int levelOfDetail() const override;

    PkString debugName() const override;

    void sigCalculationFinished(const PkRect &bounds);

private:
    KisNodeList m_nodes;
    KisSelectionSP m_selection;
    PkObject *m_requestedBy;
};

#endif // KISMOVEBOUNDSCALCULATIONJOB_H
