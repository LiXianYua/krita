#pragma once

// PkStringList 的单测类。函数定义在 test_pkstringlist.cpp。
//
// PkStringList 不是模板，所以复用不了 PkSeqTestShared.h 那份
// `template <template <typename> class Seq>` 的共享用例（它要一个类模板）。
// 基类 PkList<PkString> 的共同 API 已经由 test_pklist 压过一遍；本文件只压
// PkStringList 自己新增的那一层，外加派生类特有的两个坑：
// **新增写方法的 COW** 与 **链式操作符的类型退化**。
#include <QObject>

class PkStringListTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // 与基类 PkList<PkString> 的互转（Qt 实测两个方向都可隐式转换）
    void convertsToAndFromBaseList();
    void initializerListConstruction();

    // join —— 155 处调用点，边界压满
    void joinBoundaries();
    void joinCharSeparator();

    void filterIsSubstringMatch();
    void filterCaseInsensitive();
    void removeDuplicatesReturnsCountAndKeepsFirstOrder();
    void replaceInStringsRewritesInPlace();
    void sortOrders();

    // COW：拷贝隔离，以及**新增的写方法逐个走 PkMut()**
    void cowIsolation();
    void stringListWritersDetach();
    void constMethodsDoNotDetach();

    // 链式 << / += 之后类型不得退化成 PkList<PkString>（编得过就是证明）
    void chainedOperatorsKeepDerivedType();

    void selfAssignmentIsSafe();
};
