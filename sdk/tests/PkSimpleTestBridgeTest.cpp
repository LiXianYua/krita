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
    //
    // 状态用 shared_ptr 而非栈上的 bool：PK_COMPARE 失败会 `return;`
    // （pk/test/PkTest.h），此时队列里那个 lambda 仍持有捕获。
    // 按引用捕获栈变量会让「测试已经红了之后」的每一次 pump 写到已死的栈槽。
    // shared_ptr 让 lambda 自己拥有这份状态，与栈帧生命周期解耦。
    auto delivered = std::make_shared<bool>(false);
    PkThreadCallQueue::post(PkThread::mainThreadId(), [delivered] { *delivered = true; });

    // 显式 size_t：pendingCount() 返回 std::size_t，写字面量 1 会触发
    // -Wsign-compare（PkTestCompare.h 的 `t1 == t2`）；而 pk/test 不在本任务
    // locks 内，只能在调用点对齐类型。
    PK_COMPARE(PkThreadCallQueue::pendingCount(), std::size_t(1));
    PK_VERIFY(!*delivered);

    PK_COMPARE(PkThreadCallQueue::processPendingCalls(), 1);
    PK_VERIFY(*delivered);
}

SIMPLE_TEST_MAIN(PkSimpleTestBridgeCase)
