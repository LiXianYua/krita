/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TESTKISSWATCHGROUP_H
#define TESTKISSWATCHGROUP_H

#include <PkHash.h>
#include <PkPair.h>

// 原测试用哈希容器<std::pair<int,int>, KisSwatch>。PkHasher 的哈希扩展点只有一个
// 名字 pkHash（pk/container/PkHashFunctions.h:180 明写「不给 qHash 留回退」），而
// PkHashFunctions.h 那批重载里没有 pair 形（只有 PkStringHash.h:40 的
// pair<PkString,PkString>）。PkPair<int,int> 就是 std::pair<int,int>
// （pk/container/PkPair.h 是别名），ADL 的关联命名空间只有 std，所以这条重载必须
// 放进 namespace std 才找得到——PkHasher 的普通查找在模板定义点进行，命中不了
// 实例化点之后才可见的声明，只有 ADL 这一条路。语义照 Qt 5.15 的 qHash(std::pair)：
// hash(first, seed) ^ hash(second, ~seed)，seed=0。仅供本测试的期望镜像表使用，
// 哈希值本身不参与断言。
#include <utility>
namespace std {
inline unsigned int pkHash(const std::pair<int, int> &key) noexcept
{
    // 普通查找在第一个命中的命名空间（std）处停止，必须 :: 限定到全局的
    // pkHash(int)（PkHashFunctions.h:90 定义），否则这条 pair 重载把自己隐藏了。
    return ::pkHash(key.first) ^ ::pkHash(key.second, ~0u);
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
