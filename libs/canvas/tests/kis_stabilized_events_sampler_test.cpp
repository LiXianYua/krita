/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_stabilized_events_sampler_test.h"

#include "kis_stabilized_events_sampler.h"
#include "kis_paint_information.h"

void KisStabilizedEventsSamplerTest::test()
{
    KisStabilizedEventsSampler sampler(20);

    KisPaintInformation pi1(PkPointF(10, 10));
    KisPaintInformation pi2(PkPointF(20, 20));

    sampler.addEvent(pi1);

    QTest::qSleep(50);

    sampler.addEvent(pi2);

    QTest::qSleep(70);

    KisStabilizedEventsSampler::iterator it;
    KisStabilizedEventsSampler::iterator end;
    std::tie(it, end) = sampler.range();


    int numTotal = 0;
    int num1 = 0;
    int num2 = 0;

    for (; it != end; ++it) {
        numTotal++;
        if (it->pos().x() == 10) {
            num1++;
        } else if (it->pos().x() == 20) {
            num2++;
        }
    }

    QVERIFY(numTotal >= 6);
    QVERIFY(num1 >= 3);
    QVERIFY(num2 >= 3);
}

SIMPLE_TEST_MAIN(KisStabilizedEventsSamplerTest)
