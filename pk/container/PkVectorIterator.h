#pragma once

#include "PkJavaIterator.h"
#include "PkVector.h"

// ---------------------------------------------------------------------------
// PkVectorIterator<T> —— QVectorIterator<T> 的替代品。归属照 Qt（<QVector> 提供）。
//
// 保留范围内 7 处声明，方法用量：
//   hasNext 7 · next 5 · hasPrevious 2 · previous 2 · peekPrevious 1 ·
//   toFront 1 · toBack 2
// 实现与 PkListIterator 共用 PkJavaSeqConstIterator（理由见 PkJavaIterator.h）。
//
// **不做 PkMutableVectorIterator**：QMutableVectorIterator 全仓 0 处调用点
// （不是"保留范围内 0 处"，是**全仓** 0 处）。同理 QMutableHashIterator、
// QMutableSetIterator 也全仓 0 处，一并不做。
//
// 典型调用点：
//   libs/command/kundo2stack.cpp:403  QVectorIterator<KUndo2Command*> it(mergeCommandsVector());  ← 实参是**临时量**
//   libs/resources/KisMemoryStorage.cpp:77  QVectorIterator<KisTagSP> m_it;  ← 作为成员，构造函数初始化列表里 m_it(tags)
//   libs/image/KisAslStorage.cpp:80  new QVectorIterator<KisPSDLayerStyleSP>(m_styles)  ← 堆上构造
// 第一条要求"持拷贝"这条性质**必须真的成立**：临时容器在构造语句结束就没了，
// 迭代器持引用的话整个循环都是悬垂。
// ---------------------------------------------------------------------------

template <typename T>
using PkVectorIterator = PkJavaSeqConstIterator<PkVector<T>>;
