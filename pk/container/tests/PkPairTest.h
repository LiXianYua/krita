#pragma once

// PkPair<A,B> 的单测类。函数定义在 test_pkpair.cpp。
//
// PkPair 是 std::pair 的别名，所以这里压的**不是我们写的代码**，而是
// 「std::pair 的语义确实等于 Qt5 QPair 的语义」这个前提。brief 明确要求
// 「单测要验证这一点，别假设」——假设标准库与 Qt 的比较运算符同口径而不去压，
// 正是这次迁移最容易埋雷的地方。
#include <QObject>

class PkPairTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void firstSecondAccess();
    // 六个比较运算符都是字典序（先比 first，相等再比 second）
    void comparisonIsLexicographic();
    void qMakePairDeducesTypes();
    // 别名不是新类型：PkPair<A,B> 与 std::pair<A,B> 必须是同一个类型
    void aliasIsStdPair();
    // 能进容器（PkVector<PkPair<...>> 是真实用法，PkArrayData.cpp 已为它显式实例化）
    void worksInsideContainers();
};
