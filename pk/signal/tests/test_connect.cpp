#include "../PkObject.h"
#include "../PkConnect.h"
#include "test_util.h"

namespace {
struct Sender : PkObject {
    // 用生成器前的「手写信号」形态验证 connect/emit 机制本身。Task 4 换成生成器。
    void sigA() { PkObject::activateSignal(this, PkMemberFnKey::from(&Sender::sigA)); }
    void sigN(int v) { PkObject::activateSignal<int>(this, PkMemberFnKey::from(&Sender::sigN), v); }
};
struct Receiver : PkObject {
    int got = 0;
    void onA() { ++got; }
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
        s.sigA();  // 不得 crash、不得调用悬垂 this
        _expect(true, "emit after receiver destroy is safe (connection dead)");
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
}
