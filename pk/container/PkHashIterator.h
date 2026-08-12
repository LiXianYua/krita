#pragma once

#include "PkHash.h"
#include "PkJavaIterator.h"

// ---------------------------------------------------------------------------
// PkHashIterator<K,V> —— QHashIterator<K,V> 的替代品。归属照 Qt（<QHash> 提供）。
//
// 保留范围内 5 处声明（全部 5 处都在保留范围里），方法用量：
//   hasNext 5 · next 4 · key 3 · value 4
// 与 PkMapIterator 共用 PkJavaAssocConstIterator —— 两者除了内层容器有序无序
// 之外形状完全一样，抄两份只会制造"改一处忘一处"的漏洞面。
//
// **不做 PkMutableHashIterator**：QMutableHashIterator 全仓 0 处调用点。
//
// 典型调用点：
//   libs/image/KisAslStorage.cpp:79   new QHashIterator<QString, KoPatternSP>(m_patterns)
//   libs/widgets/KoDialog.cpp:334     QHashIterator<int, QPushButton *> it(d->mButtonList);
//   plugins/filters/halftone/KisHalftoneFilterConfiguration.cpp:24
//   plugins/generators/screentone/tests/KisScreentoneGeneratorTest.cpp:45
//
// **迭代顺序未定义**（内层是 std::unordered_map，QHash 同样不保证顺序）
// ——单测只断言"每个 key 恰好出现一次、key→value 对得上"，不断言顺序。
// ---------------------------------------------------------------------------

template <typename K, typename V>
using PkHashIterator = PkJavaAssocConstIterator<PkHash<K, V>>;
