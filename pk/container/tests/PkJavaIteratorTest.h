#pragma once

// Java 风格迭代器（PkListIterator / PkVectorIterator / PkMapIterator /
// PkHashIterator / PkMutableListIterator / PkMutableMapIterator）的单测。
// 函数定义在 test_pkjavaiterator.cpp。
//
// 最要紧的一条是"只读迭代器持**拷贝**、可变迭代器持**指针**"这条 Qt 的设计
// 差别 —— 正反两面各压一遍（readOnlyIteratorHoldsCopy /
// mutableListIteratorReallyModifiesOriginal）。
#include <QObject>

class PkJavaIteratorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // ---- 只读序列迭代器 ----
    void listIteratorForwardTraversal();
    void listIteratorBackwardTraversal();
    void vectorIteratorTraversal();
    // 持拷贝：构造后修改原容器，迭代不受影响；引用计数 2、元素零拷贝
    void readOnlyIteratorHoldsCopy();
    // 源容器是临时量 / 已经析构的局部量，迭代器照样有效
    void readOnlyIteratorOutlivesSourceContainer();

    // ---- 可变序列迭代器 ----
    void mutableListIteratorTraversal();
    void mutableListIteratorRemove();
    // 持指针：remove() 真的改到原容器，且不污染共享的另一份
    void mutableListIteratorReallyModifiesOriginal();
    // 调用点的两种构造形态：从非 const 容器拷贝初始化、按引用传参
    void mutableListIteratorConstructionShapes();

    // ---- 关联迭代器 ----
    void mapIteratorKeyValue();
    // 拷贝构造 / 拷贝赋值（KoProperties::propertyIterator 与
    // kis_kra_savexml_visitor.cpp:186 两种真实形态）
    void mapIteratorCopyAndAssign();
    void hashIteratorKeyValue();
    void mutableMapIteratorSetValue();
};
