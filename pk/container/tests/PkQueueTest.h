#pragma once

// PkQueue<T> 的单测类。函数定义在 test_pkqueue.cpp。
// 结构与 PkStackTest.h 完全对称（共享用例 + 专有方法 + 两个派生类的坑），
// 理由见那个文件的头注释。
#include <QObject>

class PkQueueTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // ---- 共同 API（在 PkQueue 上实例化一次共享用例）----
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

    // ---- PkQueue 专有 ----
    void fifoOrder();
    void headConstAndMutable();
    void queueWritersDetach();
    void chainedOperatorsKeepDerivedType();
};
