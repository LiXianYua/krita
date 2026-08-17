#include "../PkObject.h"
#include "../PkPointer.h"
#include "test_util.h"

namespace {
struct Node;
PkObject* g_lastSender = nullptr;

struct Node : PkObject {
    void sig() { PkObject::activateSignal(this, PkMemberFnKey::from(&Node::sig)); }
    void slotCheckSender() {
        g_lastSender = PkObject::sender();
        _expect(g_lastSender == this_expected, "sender() returns current emitter");
    }
    Node* this_expected = nullptr;
};
}

void run_pointer_tests()
{
    // 1. QPointer 基本：非空、data()
    {
        Node* n = new Node;
        QPointer<Node> p(n);
        _expect(!p.isNull(), "QPointer non-null before destroy");
        _expect(p.data() == n, "data() returns object");
        n->this_expected = n;
        _expect(p->this_expected == n, "operator-> returns object");
        delete n;
    }

    // 2. 探针 2：析构后 isNull + 指针置 null
    {
        Node* n = new Node;
        QPointer<Node> p(n);
        delete n;
        _expect(p.isNull(), "QPointer isNull after destroy");
        _expect(p.data() == nullptr, "QPointer data()==nullptr after destroy");
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
}
