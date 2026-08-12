#pragma once

// PkSet<T> 的单测类。PkSet 不与 PkMap/PkHash 共用实现（它没有 key→value 映射），
// 所以没有共享用例头，全部用例定义在 test_pkset.cpp。
#include <QObject>

class PkSetTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void insertAndContains();
    void removeAndClear();
    void valuesAndToList();
    void uniteIntersectSubtract();
    void comparison();
    void iterators();
    void cowIsolation();
    void copyIsConstantTime();
    void constNeverDetaches();
    void everyWriterDetaches();
    void selfAssignment();
    void moveLeavesSourceUsable();
    // qHash 链路：内建类型 / 自定义重载走 ADL / PkString
    void customQHashViaAdl();
    void pkStringElement();
};
