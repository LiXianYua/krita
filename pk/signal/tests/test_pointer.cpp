#include "../PkObject.h"
#include "../PkPointer.h"
#include "test_util.h"

namespace {
struct Node;
PkObject* g_lastSender = nullptr;
PkObject* g_outerSender = nullptr;

struct Node : PkObject {
    void sig() { PkObject::activateSignal(this, PkMemberFnKey::from(&Node::sig)); }
    void slotCheckSender() {
        g_lastSender = PkObject::sender();
        _expect(g_lastSender == this_expected, "sender() returns current emitter");
    }
    void slotEmitInner() {
        g_outerSender = PkObject::sender();
        innerEmitter->sig();
    }
    Node* this_expected = nullptr;
    Node* innerEmitter = nullptr;
};
}

void run_pointer_tests()
{
    // 1. PkPointer 基本：非空、data()
    {
        Node* n = new Node;
        PkPointer<Node> p(n);
        _expect(!p.isNull(), "PkPointer non-null before destroy");
        _expect(p.data() == n, "data() returns object");
        n->this_expected = n;
        _expect(p->this_expected == n, "operator-> returns object");
        delete n;
    }

    // 2. 探针 2：析构后 isNull + 指针置 null
    {
        Node* n = new Node;
        PkPointer<Node> p(n);
        delete n;
        _expect(p.isNull(), "PkPointer isNull after destroy");
        _expect(p.data() == nullptr, "PkPointer data()==nullptr after destroy");
    }

    // 3. sender() 返回当前 emitter（探针 3）
    {
        Node s; Node r;
        r.this_expected = &s;
        PkObject::connect(&s, &Node::sig, &r, &Node::slotCheckSender);
        g_lastSender = nullptr;
        s.sig();
        _expect(g_lastSender == &s, "sender()==emitter");
    }

    // 4. 槽外 sender() == nullptr
    {
        _expect(PkObject::sender() == nullptr, "sender() null outside emit");
    }

    // 5. 嵌套 emit：内层槽里 sender() 返回最内层 emitter（栈 LIFO）
    {
        Node a; Node b; Node r;
        r.innerEmitter = &b;
        r.this_expected = &b;
        PkObject::connect(&a, &Node::sig, &r, &Node::slotEmitInner);
        PkObject::connect(&b, &Node::sig, &r, &Node::slotCheckSender);
        g_lastSender = nullptr;
        g_outerSender = nullptr;
        a.sig();
        _expect(g_outerSender == &a, "outer slot sees outer emitter");
        _expect(g_lastSender == &b, "inner slot sees innermost emitter");
    }
}
