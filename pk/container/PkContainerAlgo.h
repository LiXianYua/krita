#pragma once

#include <type_traits>

// ---------------------------------------------------------------------------
// 容器族的两件"自由设施"：PK_FOREACH（Q_FOREACH/foreach 的等价物）与 qDeleteAll。
//
// 它们不属于任何一个容器类，但每个容器的调用点都可能用到 —— Qt 里两者都来自
// <QtGlobal>，而调用点极少单独 `#include <QtGlobal>`（靠容器头传递进来）。
// 复刻这条传递性：**每个 compat/<QtType> 垫片都 include 本文件**，与
// pk/test/compat/QObject 传递 include compat/QtGlobal 是同一手法。
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 1. PK_FOREACH —— 用量最大的单项
//
// 本仓库实测（口径：`git ls-files` 的 C/C++ 源文件，排除 pk/ 自身，全词匹配）：
//   Q_FOREACH  全仓 2291 处 / 保留范围 1543 处
//   foreach    全仓  135 处 / 保留范围  101 处
// （"保留范围" = 扣掉 `docs/实施边界-构建目标视图.md` §6 判为删除的
//   plugins/dockers|extensions|platforms|qt、krita/、libs/libkis、qmlmodules/、libs/ui。）
//
// ---- 语义：拷贝容器再迭代 ----
//
// 宏对容器做一次**拷贝**，迭代的是这份拷贝，因此**循环体内修改原容器不影响
// 本次迭代**。拷贝在 Qt 下靠隐式共享是 O(1)；PkArrayData 的拷贝同样是 O(1)
// （只拷 shared_ptr）。这两件事扣在一起，上面那 1600+ 处才不会退化成深拷贝
// ——单测 pkForeachCopyIsConstantTime 用 operator new 计数器 + 元素拷贝计数器
// 双重钉住这一条（只压其中一个维度会漏网）。
//
// ---- 为什么是双层 for 而不是 range-for ----
//
// 调用点全是 `Q_FOREACH(QString s, list)` 这种「变量声明写在宏参数里」的形态，
// range-for 表达不了它。双层 for 是 Qt 5.15 qglobal.h 的原结构，照抄：
//
//   外层：迭代游标 + control 位；内层：把 variable 绑到当前元素，只走一轮。
//
// control 位是 break/continue 都正确的全部原因，逐条走一遍：
//   · 正常结束一轮：内层增量 control=0 → 内层条件假 → 退内层 → 外层增量
//     ++i、control ^= 1（0^1=1）→ 外层条件真 → 下一轮。
//   · `continue`：作用于**内层** for，同样先跑内层增量 control=0，之后与
//     正常结束完全一样。
//   · `break`：作用于**内层** for，**跳过内层增量**，control 保持 1 →
//     内层语句结束 → 外层增量 ++i、control ^= 1（1^1=0）→ 外层条件
//     `control && ...` 为假 → 整个循环退出。
//
// ---- 嵌套 ----
//
// 内层宏展开出的 _pk_foreach_ 声明在内层 for 的作用域里，**遮蔽**外层同名对象
// ——Qt 靠的就是这条作用域遮蔽，没有别的机制。单测 pkForeachNested 证明它确实
// 工作（内层跑完后外层继续用自己的游标）。
// ---------------------------------------------------------------------------

template <typename T>
class PkForeachContainer
{
public:
    // 收 const 引用、成员按值存：实参是临时量（`Q_FOREACH(x, f())`）时也安全
    // ——c 是一份拷贝，临时量在 for-init 语句结束就析构也不影响。
    explicit PkForeachContainer(const T &t) : c(t), i(c.begin()), e(c.end()) {}

    // **c 必须是 const**：非 const 的 begin() 走 PkMut() 会 detach，那样每个
    // PK_FOREACH 都变成一次深拷贝，正好把这个宏最要紧的性质毁掉。const 之后
    // begin()/end() 解析到 const 重载 → PkConst() → 绝不 detach。
    const T c;
    // 成员声明顺序 = 初始化顺序：c 必须先于 i/e，否则 i/e 指向未构造的 c。
    typename T::const_iterator i;
    typename T::const_iterator e;
    int control = 1;
};

#define PK_FOREACH(variable, container)                                        \
    for (PkForeachContainer<std::decay_t<decltype(container)>> _pk_foreach_(   \
             (container));                                                     \
         _pk_foreach_.control && _pk_foreach_.i != _pk_foreach_.e;             \
         ++_pk_foreach_.i, _pk_foreach_.control ^= 1)                          \
        for (variable = *_pk_foreach_.i; _pk_foreach_.control;                 \
             _pk_foreach_.control = 0)

// Qt 的两个名字。`foreach` 是小写关键字风格的宏（Qt 只在 QT_NO_KEYWORDS 下不
// 定义它），保留范围内 101 处真实调用点，必须给。
//
// 加 #ifndef 守卫：调用点可能已经被别的垫片（或真 Qt 头，试接阶段）定义过，
// 重复定义成同一串文本虽然合法，但先到先得更省事、也不会在 -Wmacro-redefined
// 下噪声。
#ifndef Q_FOREACH
#define Q_FOREACH(variable, container) PK_FOREACH(variable, container)
#endif
#ifndef foreach
#define foreach(variable, container) PK_FOREACH(variable, container)
#endif

// ---------------------------------------------------------------------------
// 2. qDeleteAll —— 对容器里的**指针元素**逐个 delete
//
// 本仓库实测：全仓 139 处 / 保留范围 94 处，其中
//   · 单实参（整个容器）  93 处
//   · 双实参（begin, end）  1 处 —— plugins/paintops/hairy/hairy_brush.cpp
//                              `qDeleteAll(m_bristles.begin(), m_bristles.end())`
// 两个重载各有真实调用点，都要给。
//
// **不清空容器**：Qt 的语义就是这样（调用点通常紧跟一个 clear()），元素被 delete
// 之后容器里留的是一堆悬垂指针。别"顺手"加 clear()——那会让紧跟着 clear() 的
// 93 处调用点行为不变、却让不跟 clear() 的调用点悄悄改语义。
//
// 名字保持小写 q 前缀、不改名：与 qHash（PkHashFunctions.h）、qMakePair
// （PkPair.h）同一条口径 —— Qt 的自由函数名在本仓库里原样保留，垫片不映射它们。
// ---------------------------------------------------------------------------

template <typename PkForwardIt>
void qDeleteAll(PkForwardIt begin, PkForwardIt end)
{
    while (begin != end) {
        delete *begin;
        ++begin;
    }
}

// 收 const 引用 → begin()/end() 解析到 const 重载 → 不 detach。
// 关联容器上 `*it` 给的是 **value**（PkAssocIterator 的形状），所以
// qDeleteAll(map) 删的是 value 那一侧的指针 —— 与 Qt 一致。
template <typename PkContainer>
void qDeleteAll(const PkContainer &c)
{
    qDeleteAll(c.begin(), c.end());
}
