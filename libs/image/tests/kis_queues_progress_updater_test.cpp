/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_queues_progress_updater_test.h"

#include <simpletest.h>

#include "kis_queues_progress_updater.h"
#include <testutil.h>
#include <PkThreadCallQueuePumpHost.h>

// 等待必须用 KritaTestSdk::waitFor() 而不是 QTest::qWait()：QTest::qWait 走 Qt 的
// 事件循环，而 KisQueuesProgressUpdater 的 sigStartTicking/sigStopTicking
// （PkConnectionType::Queued）与 PkTimer 的回调都只经 PkThreadCallQueue 投递——
// Qt 的事件循环不驱动 pk 队列，等待期间不 pump 会把这两条一起冻住。

void KisQueuesProgressUpdaterTest::testSlowProgress()
{
    TestUtil::TestProgressBar progressProxy;
    KisQueuesProgressUpdater updater(&progressProxy);

    updater.updateProgress(200, "test task");
    updater.updateProgress(100, "test task");

    KritaTestSdk::waitFor(100);

    QCOMPARE(progressProxy.min(), 0);
    QCOMPARE(progressProxy.max(), 0);
    QCOMPARE(progressProxy.value(), 0);
    QCOMPARE(progressProxy.format(), QString());

    KritaTestSdk::waitFor(500);

    QCOMPARE(progressProxy.min(), 0);
    QEXPECT_FAIL("", "The max should be 200 but is 0.", Continue);
    QCOMPARE(progressProxy.max(), 200);
    QEXPECT_FAIL("", "Progress should be 100 but is 0.", Continue);
    QCOMPARE(progressProxy.value(), 100);
    QEXPECT_FAIL("", "format() should be 'test task' but is empty.", Continue);
    QCOMPARE(progressProxy.format(), QString("test task"));

    updater.updateProgress(0, "test task");

    KritaTestSdk::waitFor(500);

    QCOMPARE(progressProxy.min(), 0);
    QEXPECT_FAIL("", "Max should be 200 but is 100.", Continue);
    QCOMPARE(progressProxy.max(), 200);
    QEXPECT_FAIL("", "Value should be 200 but is 100.", Continue);
    QCOMPARE(progressProxy.value(), 200);
    QEXPECT_FAIL("", "format() should be 'test task' but is '%p%'.", Continue);
    QCOMPARE(progressProxy.format(), QString("test task"));
}

void KisQueuesProgressUpdaterTest::testFastProgress()
{
    /**
     * If the progress is too fast we don't even touch the bar
     */

    TestUtil::TestProgressBar progressProxy;
    KisQueuesProgressUpdater updater(&progressProxy);

    updater.updateProgress(200, "test task");
    updater.updateProgress(0, "test task");

    KritaTestSdk::waitFor(20);

    QCOMPARE(progressProxy.min(), 0);
    QEXPECT_FAIL("", "Max should be 0 but is 100.", Continue);
    QCOMPARE(progressProxy.max(), 0);
    QEXPECT_FAIL("", "Value should be 0 but is 100.", Continue);
    QCOMPARE(progressProxy.value(), 0);
    QEXPECT_FAIL("", "format() should be empty but is '%p%'.", Continue);
    QCOMPARE(progressProxy.format(), QString());

    updater.updateProgress(100, "test task");
    updater.updateProgress(0, "test task");

    KritaTestSdk::waitFor(20);

    QCOMPARE(progressProxy.min(), 0);
    QEXPECT_FAIL("", "Max should be 0 but is 100.", Continue);
    QCOMPARE(progressProxy.max(), 0);
    QEXPECT_FAIL("", "Value should be 0 but is 100.", Continue);
    QCOMPARE(progressProxy.value(), 0);
    QEXPECT_FAIL("", "format() should be empty but is '%p%'.", Continue);
    QCOMPARE(progressProxy.format(), QString());

    updater.updateProgress(0, "test task");
    updater.updateProgress(0, "test task");

    KritaTestSdk::waitFor(20);

    QCOMPARE(progressProxy.min(), 0);
    QEXPECT_FAIL("", "Max should be 0 but is 100.", Continue);
    QCOMPARE(progressProxy.max(), 0);
    QEXPECT_FAIL("", "Value should be 0 but is 100.", Continue);
    QCOMPARE(progressProxy.value(), 0);
    QEXPECT_FAIL("", "format() should be empty but is '%p%'.", Continue);
    QCOMPARE(progressProxy.format(), QString());

    KritaTestSdk::waitFor(500);

    QCOMPARE(progressProxy.min(), 0);
    QEXPECT_FAIL("", "Max should be 0 but is 100.", Continue);
    QCOMPARE(progressProxy.max(), 0);
    QEXPECT_FAIL("", "Value should be 0 but is 100.", Continue);
    QCOMPARE(progressProxy.value(), 0);
    QEXPECT_FAIL("", "format() should be empty but is '%p%'.", Continue);
    QCOMPARE(progressProxy.format(), QString());
}

SIMPLE_TEST_MAIN(KisQueuesProgressUpdaterTest)
