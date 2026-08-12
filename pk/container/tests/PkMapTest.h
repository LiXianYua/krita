#pragma once

// PkMap<K,V> 的单测类。形状与真实 Krita 测试类一致：`#include <QObject>`
// 解析到 pk/test/compat/QObject（-I pk/test/compat），Q_OBJECT 展开成 friend
// 模板。函数定义在 test_pkmap.cpp。
//
// 共同 API（与 PkHash 共用一份实现）的用例住在 tests/PkAssocTestShared.h，
// 这里的每个槽只是把它在 PkMap 上实例化一次；PkMap 专有的**有序性**与
// lowerBound/upperBound 在本文件末尾单列。
#include <QObject>

class PkMapTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // ---- 共同 API ----
    void lookupAndDefaults();
    void subscript();
    void insertTakeRemove();
    void iteratorShape();
    void iteratorTraversal();
    void iteratorConversion();
    void findAndErase();
    void keysAndValues();
    void comparison();
    void cowIsolation();
    void copyIsConstantTime();
    void constNeverDetaches();
    void iteratorDetachTiming();
    void everyWriterDetaches();
    void selfAssignment();
    void moveLeavesSourceUsable();

    // ---- PkMap 专有 ----
    // 按 key 升序（QHash 无序，这条只有 QMap 有）
    void orderedByKey();
    void lowerAndUpperBound();
    // 有序容器上 erase 返回的"下一个"是确定的，可以逐条对上实测
    void eraseReturnsNextKey();
    // 专有的写方法同样逐个验证「共享状态下调用之后两边不再共享」
    void mapWritersDetach();
};
