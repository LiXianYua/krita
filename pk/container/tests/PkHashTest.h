#pragma once

// PkHash<K,V> 的单测类。共同 API 的用例住在 tests/PkAssocTestShared.h，这里的
// 每个槽只是把它在 PkHash 上实例化一次；PkHash 专有的那批（qHash 链路、
// 嵌套容器）在本文件末尾单列。函数定义在 test_pkhash.cpp。
#include <QObject>

class PkHashTest : public QObject
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

    // ---- PkHash 专有 ----
    // 内建类型的 qHash 重载（Qt 本来就提供，调用点指望它们存在）
    void builtinQHash();
    // **本任务最容易在集成时才炸的一条**：Krita 全仓 18 处自定义
    // `uint qHash(const X &)` 重载靠非限定查找 + ADL 命中
    void customQHashViaAdl();
    // PkHash<PkString, V> 是高频用法，重载写在 pk/container/PkStringHash.h
    void pkStringKey();
    // 试接目标 kis_fill_interval_map_test.cpp 的用法：
    // QHash<int, QMap<int, POD>> + it->insert(...) + it->field
    void nestedHashOfMap();
    // QHash **没有** lowerBound/upperBound —— 编译期钉住，别让它长出来
    void noOrderedApi();
};
