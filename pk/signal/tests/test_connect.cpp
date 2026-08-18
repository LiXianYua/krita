#include "../PkObject.h"
#include "../PkConnect.h"
#include "../../concurrent/PkThread.h"
#include "test_util.h"

namespace {
struct Sender : PkObject {
    // 用生成器前的「手写信号」形态验证 connect/emit 机制本身。Task 4 换成生成器。
    void sigA() { PkObject::activateSignal(this, PkMemberFnKey::from(&Sender::sigA)); }
    void sigN(int v) { PkObject::activateSignal<int>(this, PkMemberFnKey::from(&Sender::sigN), v); }
};
struct Receiver : PkObject {
    int got = 0;
    int got2 = 0;
    void onA() { ++got; }
    void onA2() { ++got2; }
    void onN(int v) { got = v; }
};
}

void run_connect_tests()
{
    // 1. 基本连接/发射/断开（句柄式）
    {
        Sender s; Receiver r;
        PkConnection c = PkObject::connect(&s, &Sender::sigA, &r, &Receiver::onA);
        s.sigA();
        _expect(r.got == 1, "slot called once after connect+emit");
        s.sigA();
        _expect(r.got == 2, "slot called twice");
        PkObject::disconnect(c);
        s.sigA();
        _expect(r.got == 2, "slot not called after disconnect");
    }

    // 2. 带参信号
    {
        Sender s; Receiver r;
        PkObject::connect(&s, &Sender::sigN, &r, &Receiver::onN);
        s.sigN(42);
        _expect(r.got == 42, "int arg forwarded");
    }

    // 3. lambda 槽
    {
        Sender s;
        int called = 0;
        PkObject::connect(&s, &Sender::sigA, &s, [&called]{ ++called; });
        s.sigA();
        _expect(called == 1, "lambda slot called");
    }

    // 4. receiver 析构自动断连
    {
        Sender s;
        {
            Receiver* r = new Receiver;
            PkObject::connect(&s, &Sender::sigA, r, &Receiver::onA);
            s.sigA();
            _expect(r->got == 1, "called before receiver destroy");
            delete r;
        }
        // 这条的真断点是「不 crash」：emit 靠双方共享 state 的 alive 跳过 dead 条目，
        // 不触碰已析构 receiver 的内存。若想强断言，可给 Sender 加计数侧证「析构后
        // 槽不再被调」，但此处 receiver 已析构、计数无处可放，故只验证不 crash。
        s.sigA();
    }

    // 5. emit 中 disconnect（探针 4：当前 emit 只触发一次，断开下次生效）
    {
        struct R : PkObject {
            Sender* s; PkConnection* c; int count = 0;
            void onA() {
                ++count;
                if (count == 1) PkObject::disconnect(*c);
            }
        };
        Sender s; R r; r.s = &s;
        PkConnection c = PkObject::connect(&s, &Sender::sigA, &r, &R::onA);
        r.c = &c;
        s.sigA();
        s.sigA();
        _expect(r.count == 1, "disconnect inside emit stops subsequent calls");
    }

    // 6. 信号连信号（method 恰好是信号函数）
    {
        Sender s1, s2; Receiver r;
        PkObject::connect(&s1, &Sender::sigN, &s2, &Sender::sigN);  // 转发
        PkObject::connect(&s2, &Sender::sigN, &r, &Receiver::onN);
        s1.sigN(7);
        _expect(r.got == 7, "signal-to-signal forwards arg");
    }

    // 7. QOverload 重载消歧（sigN 单参 vs 假设的双参——用 sigN 与 sigA 两个不同名模拟
    //    重载场景，真重载在 Task 4 生成器 + Task 5 试接的 TestClass::sigTest2 覆盖）
    {
        Sender s; Receiver r;
        auto sig = QOverload<int>::of(&Sender::sigN);
        PkObject::connect(&s, sig, &r, &Receiver::onN);
        s.sigN(9);
        _expect(r.got == 9, "QOverload disambiguated connect");
    }

    // 8. Unique：同四元组去重（probe_unique：same-quadruple 第二次 connect 无效）
    {
        Sender s; Receiver r;
        PkConnection c1 = PkObject::connect(&s, &Sender::sigA, &r, &Receiver::onA,
                                            PkConnectionType::Unique);
        PkConnection c2 = PkObject::connect(&s, &Sender::sigA, &r, &Receiver::onA,
                                            PkConnectionType::Unique);
        _expect(c1.isValid(), "first Unique connect valid");
        _expect(!c2.isValid(), "duplicate Unique connect invalid");
        s.sigA();
        _expect(r.got == 1, "dedup: slot fires once despite duplicate Unique connect");
    }

    // 9. Unique：断开后重连同四元组成功（只查 alive 条目）
    {
        Sender s; Receiver r;
        PkConnection c1 = PkObject::connect(&s, &Sender::sigA, &r, &Receiver::onA,
                                            PkConnectionType::Unique);
        PkObject::disconnect(c1);
        PkConnection c2 = PkObject::connect(&s, &Sender::sigA, &r, &Receiver::onA,
                                            PkConnectionType::Unique);
        _expect(c2.isValid(), "reconnect after disconnect valid");
        s.sigA();
        _expect(r.got == 1, "reconnect after disconnect fires once");
    }

    // 10. Unique：不同 receiver / 不同槽成员函数不去重
    {
        Sender s; Receiver r1, r2;
        PkConnection c1 = PkObject::connect(&s, &Sender::sigA, &r1, &Receiver::onA,
                                            PkConnectionType::Unique);
        PkConnection c2 = PkObject::connect(&s, &Sender::sigA, &r2, &Receiver::onA,
                                            PkConnectionType::Unique);
        _expect(c1.isValid() && c2.isValid(), "different receiver: both Unique connects valid");
        s.sigA();
        _expect(r1.got == 1 && r2.got == 1, "different receiver: both slots fired");

        Sender s2; Receiver r;
        PkConnection c3 = PkObject::connect(&s2, &Sender::sigA, &r, &Receiver::onA,
                                            PkConnectionType::Unique);
        PkConnection c4 = PkObject::connect(&s2, &Sender::sigA, &r, &Receiver::onA2,
                                            PkConnectionType::Unique);
        _expect(c3.isValid() && c4.isValid(), "different slot: both Unique connects valid");
        s2.sigA();
        _expect(r.got == 1 && r.got2 == 1, "different slot: both slots fired");
    }

    // 11. Unique + lambda 槽：lambda 无身份不去重，两次都有效、emit 触发两次
    {
        Sender s;
        int called = 0;
        PkConnection c1 = PkObject::connect(&s, &Sender::sigA, &s,
                                            [&called]{ ++called; },
                                            PkConnectionType::Unique);
        PkConnection c2 = PkObject::connect(&s, &Sender::sigA, &s,
                                            [&called]{ ++called; },
                                            PkConnectionType::Unique);
        _expect(c1.isValid() && c2.isValid(), "lambda Unique: both connects valid");
        s.sigA();
        _expect(called == 2, "lambda Unique: no dedup, fires twice");
    }

    // 12. disconnect 三种新式形态（4 参函数指针式 / 断开全部式 / post-disconnect isValid）
    {
        // 12a. 4 参函数指针式：connect 后 disconnect(&s, &sig, &r, &slot)，emit 槽不触发
        Sender s; Receiver r;
        PkObject::connect(&s, &Sender::sigA, &r, &Receiver::onA);
        bool d1 = PkObject::disconnect(&s, &Sender::sigA, &r, &Receiver::onA);
        _expect(d1, "funcptr disconnect on live connection returns true");
        s.sigA();
        _expect(r.got == 0, "funcptr disconnect: slot not fired after 4-arg disconnect");
        // 找不到活条目返回 false
        bool d2 = PkObject::disconnect(&s, &Sender::sigA, &r, &Receiver::onA);
        _expect(!d2, "funcptr disconnect on already-dead returns false");

        // 12b. 断开全部式：同 sender 两个不同信号连到同 receiver，disconnect(&s, 0, &r, 0)
        //      （裸 0，对齐 Krita 真实拼写），两个信号 emit 都不触发
        Sender sa; Receiver ra;
        PkObject::connect(&sa, &Sender::sigA, &ra, &Receiver::onA);   // onA: ++got
        PkObject::connect(&sa, &Sender::sigN, &ra, &Receiver::onN);   // onN: got = v
        bool d = PkObject::disconnect(&sa, 0, &ra, 0);
        _expect(d, "disconnect-all returns true when at least one matched");
        sa.sigA();
        sa.sigN(42);
        _expect(ra.got == 0, "disconnect-all: neither signal fires after disconnect-all");

        // 12c. 断开全部式：不同 receiver 不受影响（同 sender 信号连 r1/r2，断 r1 全部，r2 仍触发）
        Sender sb; Receiver r1, r2;
        PkObject::connect(&sb, &Sender::sigA, &r1, &Receiver::onA);
        PkObject::connect(&sb, &Sender::sigA, &r2, &Receiver::onA);
        PkObject::disconnect(&sb, 0, &r1, 0);
        sb.sigA();
        _expect(r1.got == 0 && r2.got == 1, "disconnect-all: only r1 dead, r2 still fires");

        // 12d. post-disconnect isValid：disconnect 后句柄 isValid() 变 false
        Sender sc; Receiver rc;
        PkConnection h = PkObject::connect(&sc, &Sender::sigA, &rc, &Receiver::onA);
        _expect(h.isValid(), "handle isValid before disconnect");
        PkObject::disconnect(h);
        _expect(!h.isValid(), "handle isValid false after disconnect (Qt semantics)");
    }

    // 13. thread() 默认等于构造它的线程；moveToThread() 改写这个标记
    {
        Sender s;
        _expect(s.thread() == PkThread::currentThreadId(), "thread() defaults to the constructing thread");
        PkThreadId fakeOther{};   // 默认构造的 id，不等于任何真实线程
        s.moveToThread(fakeOther);
        _expect(s.thread() == fakeOther, "moveToThread() reassigns the affinity tag");
        _expect(s.thread() != PkThread::currentThreadId(), "affinity no longer matches the current thread after moveToThread");
    }
}
