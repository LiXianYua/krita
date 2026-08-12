#pragma once

// PkStack<T> 的单测类。函数定义在 test_pkstack.cpp。
//
// **共同 API 直接复用 tests/PkSeqTestShared.h**（那份测试是
// `template <template <typename> class Seq>`，PkStack 正好是这个形状）——
// PkStack 是 PkVector 的薄派生类，基类那套 COW / 迭代器 / 比较 / 移动语义
// 一条都不该重写第三份。本文件只额外列 PkStack 专有的 push/pop/top，
// 以及派生类特有的两个坑：**新增方法的 COW** 与**链式操作符的类型退化**。
#include <QObject>

class PkStackTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // ---- 共同 API（在 PkStack 上实例化一次共享用例）----
    void sizeIsInt();
    void sizeAndEmptiness();
    void elementAccess();
    void valueOutOfRange();
    void appendAndPrepend();
    void insertAndRemove();
    void erase();
    void search();
    void iterators();
    void constIteratorsDoNotDetach();
    void comparison();
    void streamOperators();
    void cowIsolation();
    void copyIsConstantTime();
    void constNeverDetaches();
    void everyWriterDetaches();
    void reserveDetachRules();
    void swap();
    void selfAssignment();
    void moveLeavesSourceUsable();
    void initializerListAndDefaults();

    // ---- PkStack 专有 ----
    void lifoOrder();
    void topConstAndMutable();
    // 新增的 push/pop 同样必须走 PkMut()——这是派生类最容易漏的漏洞面
    void stackWritersDetach();
    // 链式 << / += 之后类型不得退化成 PkVector（编得过就是证明）
    void chainedOperatorsKeepDerivedType();
};
