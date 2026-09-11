#include "PkSimpleTestBridgeCase.h"

#include <simpletest.h>

template <>
struct PkTestBinder<PkSimpleTestBridgeCase>
{
    static const char *className() { return "PkSimpleTestBridgeCase"; }

    static const PkTestFunction *functions()
    {
        static const PkTestFunction functions[] = {
            {"testMainThreadQueueIsReady",
             [](PkTestObject *object) {
                 static_cast<PkSimpleTestBridgeCase *>(object)->testMainThreadQueueIsReady();
             },
             nullptr},
            {"testUnpumpedCallStaysPending",
             [](PkTestObject *object) {
                 static_cast<PkSimpleTestBridgeCase *>(object)->testUnpumpedCallStaysPending();
             },
             nullptr},
        };
        return functions;
    }

    static int count() { return 2; }
    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase() { return nullptr; }
    static const PkTestFunction *cleanupTestCase() { return nullptr; }
    static const PkTestFunction *initFn() { return nullptr; }
    static const PkTestFunction *cleanupFn() { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

void PkSimpleTestBridgeCase::testMainThreadQueueIsReady()
{
    PK_VERIFY(PkThread::currentThreadId() == PkThread::mainThreadId());

    bool delivered = false;
    PkThreadCallQueue::post(PkThread::mainThreadId(), [&delivered] { delivered = true; });

    PK_COMPARE(PkThreadCallQueue::processPendingCalls(), 1);
    PK_VERIFY(delivered);
}

void PkSimpleTestBridgeCase::testUnpumpedCallStaysPending()
{
    // S线-spec「跨线程投递的 pump 是消费方必装件」的契约：post 只排队，
    // 目标线程不 pump 就静默不执行——不报错、不崩溃、不打日志。
    bool delivered = false;
    PkThreadCallQueue::post(PkThread::mainThreadId(), [&delivered] { delivered = true; });

    PK_COMPARE(PkThreadCallQueue::pendingCount(), 1);
    PK_VERIFY(!delivered);

    PK_COMPARE(PkThreadCallQueue::processPendingCalls(), 1);
    PK_VERIFY(delivered);
}

SIMPLE_TEST_MAIN(PkSimpleTestBridgeCase)
