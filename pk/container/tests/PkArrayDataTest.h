#pragma once

// COW 地基 PkArrayData<C> 的单测类。形状与真实 Krita 测试类一致：
// `#include <QObject>` 解析到 pk/test/compat/QObject（-I pk/test/compat），
// Q_OBJECT 展开成 friend 模板，让 pk_test_moc.py 生成的 PkTestBinder 能访问
// private Q_SLOTS 里的函数。函数定义在 test_arraydata.cpp。
#include <QObject>

class PkArrayDataTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // 1. 默认构造后 PkUseCount()==1
    void defaultConstructedUseCountIsOne();
    // explicit C 构造：接管内层容器，仍然独占
    void explicitInitTakesOwnership();

    // 2. 拷贝后两边 use_count==2 且互相 PkIsSharedWith
    void copyConstructShares();
    void copyAssignShares();
    // 拷贝必须是 O(1)：不拷贝任何元素（硬要求 3）
    void copyDoesNotCopyElements();

    // 3. COW 核心：一边写，另一边内容不变
    void mutDetachesAndLeavesOtherIntact();
    void detachDropsShareOnlyForCaller();

    // 4. use_count==1 时 PkMut()/PkDetach() 零拷贝（带正向对照）
    void detachOnUnsharedDoesNotCopy();
    void detachOnSharedDoesCopy();

    // 5. PkConst() 绝不 detach
    void constNeverDetaches();

    // 6. 自赋值安全
    void selfAssignmentIsSafe();
    void selfAssignmentWhileSharedIsSafe();

    // 7. 移动之后源对象可析构、可再赋值
    void moveConstructLeavesSourceUsable();
    void moveAssignLeavesSourceUsable();
};
