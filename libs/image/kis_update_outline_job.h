/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_UPDATE_OUTLINE_JOB_H
#define __KIS_UPDATE_OUTLINE_JOB_H

#include <PkColor.h>

#include "kis_spontaneous_job.h"
#include "kis_selection.h"

class KRITAIMAGE_EXPORT KisUpdateOutlineJob : public KisSpontaneousJob
{
public:
    KisUpdateOutlineJob(KisSelectionSP selection, bool updateThumbnail, const PkColor &maskColor);

    bool overrides(const KisSpontaneousJob *otherJob) override;
    void run() override;
    int levelOfDetail() const override;
    PkString debugName() const override;

private:
    KisSelectionSP m_selection;
    bool m_updateThumbnail;
    PkColor m_maskColor;
};

#endif /* __KIS_UPDATE_OUTLINE_JOB_H */
