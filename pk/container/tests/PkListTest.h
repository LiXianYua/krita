#pragma once

// PkList<T> 的单测类。共同 API 的用例住在 tests/PkSeqTestShared.h，这里的每个
// 槽只是把它在 PkList 上实例化一次；PkList 专有的 removeAt/removeAll/removeOne/
// removeFirst/removeLast/takeAt/takeFirst/takeLast/pop_back/pop_front/move/
// toVector 在本文件末尾单列。
#include <QObject>

class PkListTest : public QObject
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

    // ---- PkList 专有 ----
    void removeAtAllOne();
    void removeFirstLast();
    void takeAtFirstLast();
    void popBackFront();
    void moveElement();
    void moveElementBoundaries();
    void toVector();
    // 专有的写方法同样逐个验证「共享状态下调用之后两边不再共享」
    void listWritersDetach();
};
