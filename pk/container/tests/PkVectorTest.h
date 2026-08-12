#pragma once

// PkVector<T> 的单测类。形状与真实 Krita 测试类一致：`#include <QObject>`
// 解析到 pk/test/compat/QObject（-I pk/test/compat），Q_OBJECT 展开成 friend
// 模板。函数定义在 test_pkvector.cpp。
//
// 共同 API（与 PkList 共用一份实现）的用例住在 tests/PkSeqTestShared.h，
// 这里的每个槽只是把它在 PkVector 上实例化一次；PkVector 专有的
// resize/fill/capacity/toList 在本文件末尾单列。
#include <QObject>

class PkVectorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // ---- 共同 API ----
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
    void swap();
    void selfAssignment();
    void moveLeavesSourceUsable();
    void initializerListAndDefaults();

    // ---- PkVector 专有 ----
    void sizedConstructors();
    void resize();
    void fill();
    void capacity();
    void toList();
    // 专有的写方法同样逐个验证「共享状态下调用之后两边不再共享」
    void vectorWritersDetach();
};
