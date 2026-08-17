#include <string>
#include <utility>
#include "../PkObject.h"
#include "test_util.h"

namespace {
std::vector<std::string> g_dtorOrder;

struct Named : PkObject {
    std::string name;
    Named(std::string n, PkObject* p = nullptr) : PkObject(p), name(std::move(n)) {}
    ~Named() override { g_dtorOrder.push_back(name); }
};

// PkMemberFnKey 覆盖测试用：同一类的两个不同成员函数指针，验证 key 打包/比较语义。
struct SignalKeyProbe : PkObject {
    void methodA() {}
    void methodB() {}
};
}

void run_tree_tests()
{
    // 1. parent()/children() 挂接
    {
        PkObject root;
        PkObject a(&root);
        PkObject b(&root);
        _expect(a.parent() == &root, "child parent points to root");
        _expect(root.children().size() == 2, "root has 2 children");
    }

    // 2. 析构顺序 = 创建顺序（FIFO），探针 1 实测 c1→c2→c3
    {
        g_dtorOrder.clear();
        PkObject* root = new PkObject;
        new Named("c1", root);
        new Named("c2", root);
        new Named("c3", root);
        delete root;
        _expect(g_dtorOrder.size() == 3, "all 3 children destroyed");
        _expect(g_dtorOrder[0] == "c1" && g_dtorOrder[1] == "c2" && g_dtorOrder[2] == "c3",
                "children destroyed in creation order (FIFO), got c1->c2->c3");
    }

    // 3. 子对象析构时从 parent 摘除
    {
        PkObject* root = new PkObject;
        PkObject* a = new PkObject(root);
        _expect(root->children().size() == 1, "one child before");
        delete a;
        _expect(root->children().size() == 0, "child removed from parent on destroy");
        delete root;
    }

    // 4. aliveFlag 析构后为 false（QPointer 的地基）
    {
        PkObject* obj = new PkObject;
        auto flag = obj->aliveFlag();
        _expect(flag->load() == true, "alive before destroy");
        delete obj;
        _expect(flag->load() == false, "alive flag cleared on destroy");
    }

    // 5. PkMemberFnKey 打包语义（表示层等价替换后不破坏 key 语义）
    {
        auto a1 = PkMemberFnKey::from(&SignalKeyProbe::methodA);
        auto a2 = PkMemberFnKey::from(&SignalKeyProbe::methodA);
        auto b  = PkMemberFnKey::from(&SignalKeyProbe::methodB);
        _expect(a1 == a2, "same member fn pointer packs to equal keys");
        _expect(!(a1 == b), "different member fn pointers pack to unequal keys");
    }
}
