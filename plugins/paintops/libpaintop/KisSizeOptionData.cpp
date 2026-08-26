/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisSizeOptionData.h"


KisSizeOptionData::KisSizeOptionData(const PkString &prefix)
    : KisCurveOptionData(prefix,
          KoID("Size", PkString("Size")),
          Checkability::Checkable)
{
}

KisPaintopLodLimitations KisSizeOptionData::lodLimitations() const
{
    KisPaintopLodLimitations l;

    if (!isCheckable || isChecked) {
        // HINT: FUZZY_PER_STROKE doesn't affect instant preview
        if (sensorStruct().sensorFuzzyPerDab.isActive) {
            l.limitations.insert(KoID("size-fade", PkString("Size -> Fuzzy (sensor)")));
        }

        if (sensorStruct().sensorFade.isActive) {
            l.blockers.insert(KoID("size-fuzzy", PkString("Size -> Fade (sensor)")));
        }
    }

    return l;
}
