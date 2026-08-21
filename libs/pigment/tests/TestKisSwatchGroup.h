/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TESTKISSWATCHGROUP_H
#define TESTKISSWATCHGROUP_H

#include <PkHash.h>
#include <PkPair.h>

// 原测试用 QHash<QPair<int,int>, KisSwatch>：真 Qt 在 qhashfunctions.h 里给
// `qHash(std::pair)` 提供全局重载，PkHashFunctions 的 qHash 族没有 pair 重载
// （PkHasher 模板定义点普通查找命中不了后加的声明，只靠实例化点 ADL）。
// 这里补一份 `namespace std` 的 qHash 重载，靠 ADL 让 PkHasher 找到它——语义
// 照 Qt 5.15（hash(first, seed) ^ hash(second, ~seed)，seed=0）。仅供本测试的
// 期望镜像表使用，哈希值本身不参与断言。
#include <utility>
namespace std {
inline unsigned int qHash(const std::pair<int, int> &key) noexcept
{
    // 普通查找在第一个命中的命名空间（std）处停止，必须 :: 限定到全局的
    // qHash(int)（PkHashFunctions.h 定义），否则 pair 重载把自己隐藏了。
    return ::qHash(key.first) ^ ::qHash(key.second, ~0u);
}
}

#include <KisSwatchGroup.h>

#include <PkTest.h>
// Q_OBJECT / private Q_SLOTS: 的 token 留给 pk_test_moc.py 扫描（Q 后跟 _ 不命中判据正则）。
// 宏展开与 pk/test/compat 的 Q_OBJECT 垫片同构；.cpp 经 <simpletest.h> 引入的 compat 定义与此相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

class TestKisSwatchGroup : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void testAddingOneEntry();
    void testAddingMultipleEntries();
    void testReplaceEntries();
    void testRemoveEntries();
    void testChangeColumnNumber();
    void testAddEntry();

    void testName();
    void testColorCount();
    void testInfoList();

private:

    KoColor blue();
    KoColor red();

    KisSwatchGroup g;
    PkHash<PkPair<int, int>, KisSwatch> testSwatches;
};


#endif /* TESTKISSWATCHGROUP_H */
