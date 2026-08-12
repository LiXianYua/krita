// PkEventSink 的测试。用 pk/test（R-11）的 PK_* 宏 + 真实的 PkTest::qExec
// 派发，形态照抄 test_stream.cpp（见那个文件顶部注释解释为什么不经
// pk_test_moc.py：测试类成员是 public，PkTestBinder<T> 特化手写即可）。
//
// imageUpdated(const PkRect &) 未被本文件的任何用例调用——PkRect 归 R-03
// （未交付），外部没法构造一个实参传给它。见 PkEventSink.h 里该方法的注释
// 与 pk/port/README.md 的登记表。本文件只验证剩下 8 个事件的调度语义，这
// 已经覆盖了 brief 要求的两条断言。
#include "../PkEventSink.h"
#include "PkTest.h"

#include <string>
#include <vector>

namespace {

// KisNode 只前置声明，从不解引用——测试里用一个假指针值当"身份"来比对，
// 这正是裸指针参数在同步通知场景下该有的用法：调用方只关心"是不是同一个
// 节点"，不关心节点本身的内容。
KisNode *const kFakeParent = reinterpret_cast<KisNode *>(0x1000);
KisNode *const kFakeNode = reinterpret_cast<KisNode *>(0x2000);

// ① 只 override 一个事件：其余 7 个走基类的空默认实现，必须编得过、且不
// override 的事件被调用时不能崩溃（空函数体本来就什么也不做，能跑到底就
// 是通过）。
class MinimalOverrideSink : public PkEventSink
{
public:
    bool nodeChangedCalled = false;
    KisNode *lastNode = nullptr;

    void nodeChanged(KisNode *node) override
    {
        nodeChangedCalled = true;
        lastNode = node;
    }
};

// ② 记录全部调用顺序，用来断言成对事件（aboutToXxx/xxxHasBeenDone）严格按
// "之前→之后"的顺序被调用，以及单个事件之间不互相打乱顺序。
class RecordingSink : public PkEventSink
{
public:
    std::vector<std::string> calls;
    // 评审 I-3：nodeHasBeenAdded 新增的 dontActivateNode 位，记录最近一次
    // 收到的值，用来断言这一位被真正转发（不是被端口默默吞掉）。
    bool lastDontActivateNode = false;

    void aboutToAddANode(KisNode *, int) override { calls.push_back("aboutToAddANode"); }
    void nodeHasBeenAdded(KisNode *, int, bool dontActivateNode = false) override
    {
        calls.push_back("nodeHasBeenAdded");
        lastDontActivateNode = dontActivateNode;
    }
    void aboutToRemoveANode(KisNode *, int) override { calls.push_back("aboutToRemoveANode"); }
    void nodeHasBeenRemoved(KisNode *, int) override { calls.push_back("nodeHasBeenRemoved"); }
    void aboutToMoveNode(KisNode *, int, int) override { calls.push_back("aboutToMoveNode"); }
    void nodeHasBeenMoved(KisNode *, int, int) override { calls.push_back("nodeHasBeenMoved"); }
    void nodeChanged(KisNode *) override { calls.push_back("nodeChanged"); }
    void historyStateChanged() override { calls.push_back("historyStateChanged"); }
    // 评审 I-4：libs/image/commands/kis_image_change_layers_command.cpp 的
    // redo/undo 整棵图层栈替换事件,不伴随任何 per-node 增删事件。
    void layersChanged() override { calls.push_back("layersChanged"); }
};

} // namespace

class PkEventSinkTestCase : public PkTestObject
{
public:
    // ① 只 override nodeChanged：验证「virtual + 空默认实现」的全部意义
    // ——子类不写的事件也能编过、跑起来不崩，写了的那个能被正确路由到。
    void testMinimalOverrideCompilesAndDispatches();

    // 未 override 任何事件的子类也必须能实例化、调用不崩——空默认实现的
    // 另一半意义：不 override 任何东西也是合法用法，不是必须至少 override
    // 一个才行。
    void testNoOverrideAtAllStillCallable();

    // ② 成对事件按调用顺序被记录，覆盖三对 aboutToXxx/xxxHasBeenDone。
    void testAddPairCalledInOrder();
    void testRemovePairCalledInOrder();
    void testMovePairCalledInOrder();

    // ② 混合调用：图层树事件与撤销栈事件交替调用，记录的顺序必须原样保留
    // ——证明这是「同一个虚函数表按调用顺序分发」，不是按事件类型分桶。
    void testMixedEventsPreserveCallOrder();

    // 裸指针参数只传递身份，不解引用：同一个假指针值原样传到 override。
    void testNodePointerIdentityIsPreserved();

    // 评审 I-3：nodeHasBeenAdded 新增的 dontActivateNode 位必须被转发到
    // override，不能被端口默默吞掉——默认值 false、显式传 true 都要能收到。
    void testNodeHasBeenAddedForwardsDontActivateNodeFlag();

    // 评审 I-4：layersChanged() 是独立事件，不伴随任何 per-node 增删事件，
    // 与其它事件混调时顺序必须原样保留。
    void testLayersChangedIsIndependentEventPreservingOrder();
};

void PkEventSinkTestCase::testMinimalOverrideCompilesAndDispatches()
{
    MinimalOverrideSink sink;
    PkEventSink &base = sink;   // 通过基类引用调用，验证走的是虚函数分发。

    // 未 override 的事件：走基类空实现，不应该影响 sink 自己的状态。
    base.aboutToAddANode(kFakeParent, 0);
    PK_VERIFY(!sink.nodeChangedCalled);

    // override 的事件：必须被正确路由到子类实现。
    base.nodeChanged(kFakeNode);
    PK_VERIFY(sink.nodeChangedCalled);
}

void PkEventSinkTestCase::testNoOverrideAtAllStillCallable()
{
    PkEventSink sink;   // 直接实例化基类本身：验证它不是抽象类（design choice①），
                        // 这一步能编译通过本身就是证据——若有未定义的纯虚函数，
                        // 这行会在编译期报错，不需要额外断言。
    sink.aboutToAddANode(kFakeParent, 0);
    sink.nodeHasBeenAdded(kFakeParent, 0);
    sink.aboutToRemoveANode(kFakeParent, 1);
    sink.nodeHasBeenRemoved(kFakeParent, 1);
    sink.aboutToMoveNode(kFakeNode, 0, 2);
    sink.nodeHasBeenMoved(kFakeNode, 0, 2);
    sink.nodeChanged(kFakeNode);
    sink.historyStateChanged();
    sink.layersChanged();

    // 评审 M-3：上面全跑一遍不崩溃只证明"没有崩"，PK_VERIFY(true) 是恒真
    // 断言、测不出任何回归。换一个可观测通道：RecordingSink 记录调用序列，
    // 断言调用次数——这样"某个事件的空默认实现被误删/漏写"会让这里变红。
    RecordingSink recorder;
    recorder.aboutToAddANode(kFakeParent, 0);
    recorder.nodeHasBeenAdded(kFakeParent, 0);
    recorder.aboutToRemoveANode(kFakeParent, 1);
    recorder.nodeHasBeenRemoved(kFakeParent, 1);
    recorder.aboutToMoveNode(kFakeNode, 0, 2);
    recorder.nodeHasBeenMoved(kFakeNode, 0, 2);
    recorder.nodeChanged(kFakeNode);
    recorder.historyStateChanged();
    recorder.layersChanged();
    PK_COMPARE((int)recorder.calls.size(), 9);
}

void PkEventSinkTestCase::testAddPairCalledInOrder()
{
    RecordingSink sink;
    sink.aboutToAddANode(kFakeParent, 3);
    sink.nodeHasBeenAdded(kFakeParent, 3);

    PK_COMPARE((int)sink.calls.size(), 2);
    PK_COMPARE(sink.calls[0], std::string("aboutToAddANode"));
    PK_COMPARE(sink.calls[1], std::string("nodeHasBeenAdded"));
}

void PkEventSinkTestCase::testRemovePairCalledInOrder()
{
    RecordingSink sink;
    sink.aboutToRemoveANode(kFakeParent, 2);
    sink.nodeHasBeenRemoved(kFakeParent, 2);

    PK_COMPARE((int)sink.calls.size(), 2);
    PK_COMPARE(sink.calls[0], std::string("aboutToRemoveANode"));
    PK_COMPARE(sink.calls[1], std::string("nodeHasBeenRemoved"));
}

void PkEventSinkTestCase::testMovePairCalledInOrder()
{
    RecordingSink sink;
    sink.aboutToMoveNode(kFakeNode, 0, 5);
    sink.nodeHasBeenMoved(kFakeNode, 0, 5);

    PK_COMPARE((int)sink.calls.size(), 2);
    PK_COMPARE(sink.calls[0], std::string("aboutToMoveNode"));
    PK_COMPARE(sink.calls[1], std::string("nodeHasBeenMoved"));
}

void PkEventSinkTestCase::testMixedEventsPreserveCallOrder()
{
    RecordingSink sink;
    sink.aboutToAddANode(kFakeParent, 0);
    sink.nodeHasBeenAdded(kFakeParent, 0);
    sink.historyStateChanged();          // 撤销栈事件穿插在图层树事件之间。
    sink.nodeChanged(kFakeNode);
    sink.historyStateChanged();

    const std::vector<std::string> expected = {
        "aboutToAddANode", "nodeHasBeenAdded", "historyStateChanged",
        "nodeChanged", "historyStateChanged"
    };
    PK_COMPARE((int)sink.calls.size(), (int)expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        PK_COMPARE(sink.calls[i], expected[i]);
    }
}

void PkEventSinkTestCase::testNodePointerIdentityIsPreserved()
{
    MinimalOverrideSink sink;
    sink.nodeChanged(kFakeNode);
    PK_VERIFY(sink.lastNode == kFakeNode);
}

void PkEventSinkTestCase::testNodeHasBeenAddedForwardsDontActivateNodeFlag()
{
    RecordingSink sink;

    sink.nodeHasBeenAdded(kFakeParent, 0);   // 不传：默认值 false（对应真 Qt
                                              // "新增图层后照常激活"的默认行为）。
    PK_VERIFY(!sink.lastDontActivateNode);

    sink.nodeHasBeenAdded(kFakeParent, 1, true);   // 显式传 true 必须被转发到 override。
    PK_VERIFY(sink.lastDontActivateNode);
}

void PkEventSinkTestCase::testLayersChangedIsIndependentEventPreservingOrder()
{
    RecordingSink sink;
    sink.aboutToAddANode(kFakeParent, 0);
    sink.nodeHasBeenAdded(kFakeParent, 0);
    sink.layersChanged();          // 拼合/撤销触发的整棵图层栈替换，穿插在
                                    // per-node 事件之间，不改变调用顺序。
    sink.nodeChanged(kFakeNode);

    const std::vector<std::string> expected = {
        "aboutToAddANode", "nodeHasBeenAdded", "layersChanged", "nodeChanged"
    };
    PK_COMPARE((int)sink.calls.size(), (int)expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        PK_COMPARE(sink.calls[i], expected[i]);
    }
}

// PkTestBinder<PkEventSinkTestCase> 特化——手写，形状对照
// pk/test/pk_test_moc.py 的 emit_binder() 输出（同 test_stream.cpp 的做法）。
template <>
struct PkTestBinder<PkEventSinkTestCase> {
    static const char *className() { return "PkEventSinkTestCase"; }

    static const PkTestFunction *functions()
    {
        static const PkTestFunction fns[] = {
            {"testMinimalOverrideCompilesAndDispatches",
             [](PkTestObject *o) {
                 static_cast<PkEventSinkTestCase *>(o)->testMinimalOverrideCompilesAndDispatches();
             },
             nullptr},
            {"testNoOverrideAtAllStillCallable",
             [](PkTestObject *o) { static_cast<PkEventSinkTestCase *>(o)->testNoOverrideAtAllStillCallable(); },
             nullptr},
            {"testAddPairCalledInOrder",
             [](PkTestObject *o) { static_cast<PkEventSinkTestCase *>(o)->testAddPairCalledInOrder(); },
             nullptr},
            {"testRemovePairCalledInOrder",
             [](PkTestObject *o) { static_cast<PkEventSinkTestCase *>(o)->testRemovePairCalledInOrder(); },
             nullptr},
            {"testMovePairCalledInOrder",
             [](PkTestObject *o) { static_cast<PkEventSinkTestCase *>(o)->testMovePairCalledInOrder(); },
             nullptr},
            {"testMixedEventsPreserveCallOrder",
             [](PkTestObject *o) { static_cast<PkEventSinkTestCase *>(o)->testMixedEventsPreserveCallOrder(); },
             nullptr},
            {"testNodePointerIdentityIsPreserved",
             [](PkTestObject *o) { static_cast<PkEventSinkTestCase *>(o)->testNodePointerIdentityIsPreserved(); },
             nullptr},
            {"testNodeHasBeenAddedForwardsDontActivateNodeFlag",
             [](PkTestObject *o) {
                 static_cast<PkEventSinkTestCase *>(o)->testNodeHasBeenAddedForwardsDontActivateNodeFlag();
             },
             nullptr},
            {"testLayersChangedIsIndependentEventPreservingOrder",
             [](PkTestObject *o) {
                 static_cast<PkEventSinkTestCase *>(o)->testLayersChangedIsIndependentEventPreservingOrder();
             },
             nullptr},
        };
        return fns;
    }
    static int count() { return 9; }

    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase() { return nullptr; }
    static const PkTestFunction *cleanupTestCase() { return nullptr; }
    static const PkTestFunction *initFn() { return nullptr; }
    static const PkTestFunction *cleanupFn() { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

int run_eventsink_tests(int argc, char **argv)
{
    PkEventSinkTestCase tc;
    return PkTest::qExec(&tc, argc, argv);
}
