#pragma once

// PK_FOREACH（Q_FOREACH / foreach 的等价物）与 qDeleteAll 的单测。
// 函数定义在 test_pkforeach.cpp。
//
// PK_FOREACH 是整个容器族用量最大的单项（保留范围 1543 + 101 处），它的性质
// 全靠"容器拷贝是 O(1)"撑着 —— 所以本文件里最要紧的一条是
// copyIsConstantTime：**堆分配计数器与元素拷贝计数器双重钉住**，只压其中一个
// 维度会漏网。
#include <QObject>

class PkForeachTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // 1. 迭代顺序与内容正确（序列 / 关联 / 集合 / 派生类各一遍）
    void iterationOrderAndContent();
    // 2. 循环体内修改原容器不影响本次迭代 —— 拷贝语义的核心
    void bodyMutationDoesNotAffectIteration();
    // 3. 拷贝是 O(1)：循环期间引用计数为 2、元素零拷贝、堆分配零次
    void copyIsConstantTime();
    // 3b. 按值绑定的循环变量该拷几次就拷几次（证明上一条不是"计数器根本没工作"）
    void byValueLoopVariableCopiesEachElementOnce();
    // 4. break / continue 都正确（含嵌套里 break 内层）
    void breakAndContinue();
    // 5. 嵌套 PK_FOREACH 正确（内层的 _pk_foreach_ 遮蔽外层）
    void nested();
    // 6. 空容器不进循环体
    void emptyContainerSkipsBody();
    // 7. Q_FOREACH / foreach 两个 Qt 名字都在，且与 PK_FOREACH 等价
    void qtSpellingsWork();

    // ---- qDeleteAll ----
    // 每个元素恰好 delete 一次，且**不清空容器**（Qt 语义）
    void qDeleteAllDeletesEachElementOnce();
    // 双实参 (begin, end) 重载（hairy_brush.cpp 的形态）
    void qDeleteAllIteratorRangeOverload();
    // 关联容器上删的是 value 那一侧
    void qDeleteAllOnAssociativeContainers();
};
