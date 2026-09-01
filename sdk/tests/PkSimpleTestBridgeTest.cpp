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
        };
        return functions;
    }

    static int count() { return 1; }
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

SIMPLE_TEST_MAIN(PkSimpleTestBridgeCase)
