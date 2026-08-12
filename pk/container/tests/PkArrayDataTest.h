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
    // 1. 默认构造：空、可用、走共享空哨兵（零分配），首次写入分裂成独占
    //
    //    **不要在这里断言 PkUseCount() 等于某个具体数**：默认构造的容器指向
    //    进程内共享的空哨兵，计数是「1 + 当前活着的空实例个数」，随执行顺序浮动。
    //    要断言的是真实语义——空、可读、可写、写后独占、彼此互不影响。
    void defaultConstructedIsEmptyAndUsable();
    void defaultConstructedSplitsToExclusiveOnFirstWrite();
    // explicit C 构造：接管内层容器，仍然独占（哨兵路径的正向对照）
    void explicitInitTakesOwnership();
    // 且是真的「接管」——不得拷贝元素
    void explicitInitDoesNotCopyElements();

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

    // 7. 移动：源是「空且完全可用」的容器（Qt 语义），走共享空哨兵
    //    最小冒烟断言放在移动组最前面：这一组一旦回归成解空指针，进程会崩，
    //    有它至少能在输出里看到崩在移动组、而不是一片空白。
    void moveSmokeSourceObserversDoNotCrash();
    void moveConstructLeavesSourceEmptyAndUsable();
    void moveAssignLeavesSourceEmptyAndUsable();
    void moveIsConstantTime();
    void moveCarriesSharingToTarget();
    void selfMoveAssignmentIsSafe();
    void movedFromSourceIsIndependentOfTarget();
    // 哨兵不会被写污染：多个 moved-from 各自写入互不影响
    void movedFromContainersDoNotShareSentinelOnWrite();

    // PkSwap：零分配、noexcept，Task 2–6 实现 Qt 的 swap() 走它
    void swapExchangesBuffers();

    // 8. 堆分配探针 —— 把「零分配」的承诺变成有牙的断言。
    //    docs/Qt替代品选型.md §5 点名的唯一性能不确定项就在这一组里。
    void copyDoesNotAllocate();
    // 默认构造是全代码库最高频的操作，Qt 下是零堆分配（指向 shared_null）。
    // 这是本组里最有牙的一条：实现一旦退回 make_shared<C>()，只有它会响。
    void defaultConstructionDoesNotAllocate();
    void moveDoesNotAllocate();
    void swapDoesNotAllocate();
    void detachAllocationCounts();
};
