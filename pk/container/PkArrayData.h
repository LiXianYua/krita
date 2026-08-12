#pragma once

#include <memory>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// PkArrayData<C> —— COW（写时复制）地基。C 是内层标准容器
// （std::vector<T> / std::map<K,V> / std::unordered_map<K,V> / ...）。
//
// 名字一律 Pk 前缀：它不是用量表里的公开 API，只是地基。
//
// ---- 全项目共用一份 COW 地基（线级 spec「已裁决的岔路」）----
//
// 文件住在 pk/container/，但**字符串与容器同源**：`PkString` 的 buffer 是
// std::vector<char16_t>，所以它直接用 `PkArrayData<std::vector<char16_t>>`。
// 对应 Qt 的做法（实测 Qt 5.15）：QArrayData → QTypedArrayData<T> →
// QString::Data = QTypedArrayData<ushort> / QVector<T>::Data = QTypedArrayData<T>。
//
// **理由不是省代码，是 detach 的时机可观测**——迭代器何时失效、写共享实例何时
// 真拷贝。两套 COW 实现就是两次把 detach 时机写歪的机会。Qt 靠共用原语保证
// 一致，不是靠两处各自小心。
//
// 本地基按**内层容器 C** 参数化，而不是按元素类型 T。比 Qt 更通用：QMap 在 Qt
// 里用的是另一套 QMapData，不共用 QArrayData；这里 PkMap/PkHash/PkSet
// （内层 std::map / unordered_map / unordered_set）与序列容器共用同一份。
//
// **迁移归属**：`pk/string` 改用本地基由 R-13 做（它本来就要动 pk/string），
// 不在 R-02 范围内。R-02 只保证「形态可复用」，并用一条显式实例化
// （PkArrayData.cpp 里的 std::vector<char16_t>）把这件事变成编译期验证过的事实。
//
// ---- 对 C 的要求（下面有 static_assert 钉住，不只是注释）----
//
// 1. **可默认构造** —— 默认构造与共享空哨兵都要 make_shared<C>()
// 2. **可拷贝构造** —— PkDetach() 的深拷贝是 make_shared<C>(*d)
//
// 只有这两条是硬要求。`explicit PkArrayData(C init)` 的按值形参会用到移动构造，
// 但 C 不可移动时会自动退回拷贝，仍然编得过（只是多一次拷贝）。
//
// **这两条 static_assert 覆盖不到的一格**：标准库容器的拷贝构造是**无条件声明**
// 的，所以 `std::is_copy_constructible<std::vector<MoveOnly>>` 实测为 **true**，
// 断言放行，真正的错误要等实例化到 stl_uninitialized.h 深处才报。也就是说
// 「元素类型不可拷贝」这一格靠的还是原来那种难读的模板报错。被挡住的是
// C 本身不可拷贝/不可默认构造（例如误传了 std::unique_ptr）——这两种都验证过会响。
//
// 为什么 CoW 不是可选项：Krita 全仓 Q_FOREACH(2157) + foreach(129) = 2286 处
// 按值拷贝整个容器，Qt 下靠隐式共享是 O(1)。地基不做 CoW，这 2286 处全部变深拷贝。
//
// 四条硬要求（Task 2–7 的容器实现照此消费，不许各写各的）：
//
// 1. PkMut() 是**唯一**的写入口。每个容器的非 const 方法都必须经它拿到内层引用。
//    绕过它直接碰 d 就是 COW 漏洞——共享的两个容器会互相污染。
// 2. PkConst() **绝不** detach。const 方法、constBegin/constEnd/cbegin/cend 全走它。
// 3. 拷贝构造/赋值必须 O(1)（只拷 shared_ptr）——见上面 2286 处。
// 4. PkUseCount()/PkIsSharedWith() **只给单测用**，不进 compat/ 垫片
//    （isDetached()/isSharedWith() 在 Krita 调用点实测都是 0 处）。
//
// ---- detach 语义：逐条对齐 Qt（线级 spec 说这几条将来会被对拍咬到）----
//
// 1. **共享空实例**：moved-from 的源指向 PkSharedEmpty() 返回的那一份进程内
//    共享空 C —— 对应 Qt 的 QArrayData::shared_null。往它上面写会 detach，
//    所以它永远不会被污染（机制见下一节）。
// 2. **引用计数是原子的**：由 std::shared_ptr 保证（对应 Qt 的 QtPrivate::RefCount
//    用 QAtomicInt）。多个线程各自持有同一份缓冲区的拷贝、各自 detach 是安全的；
//    **但同一个 PkArrayData 实例本身不是线程安全的**，与 Qt 的隐式共享容器同口径。
// 3. **写时分裂的判据是 use_count() > 1**：引用计数 >1 就深拷贝，==1 时零成本。
//    对应 Qt 的 `if (d->ref.isShared()) reallocData(...)`。判据只看引用计数，
//    不看别的（不看容量、不看是否 const）——这一条决定了 detach 发生的**时机**，
//    也就决定了迭代器何时失效、写共享实例何时真拷贝。
//
// ---- 移动语义：共享空哨兵（与 Qt 的 QArrayData::shared_null 同构）----
//
// 移动之后的源对象是**空容器且完全可用**——PkConst()/PkMut()/PkDetach()/
// PkUseCount() 全都能照常调。做法是让源指向一个**进程内共享的空 C**
// （PkSharedEmpty()），而不是给它新分配一个空的。这样移动是
// **noexcept + 零堆分配**，与 Qt 的 QVector/QList 一致
// （`is_nothrow_move_constructible` 在 Qt 那边是 true，单测里有 static_assert 钉住）。
//
// **往哨兵上写会自动 detach，所以哨兵不会被污染**：哨兵那个 static shared_ptr
// 自己长期持有 1 份引用，任何指向它的容器 use_count() 都是 1+N ≥ 2，
// PkDetach() 的 `use_count() > 1` 判据**照常成立**，第一次写入就分裂出自己的副本。
// 这正是 Qt 往 sharedNull 上写的行为。单测有一条专门压这个
// （两个 moved-from 各自写入，互不影响）。
//
// **不要改用 shared_ptr 的 aliasing 构造来省掉哨兵那次分配**：aliasing 出来的
// shared_ptr 没有控制块，use_count() 是 0，上面那条 `> 1` 判据会失效 ——
// 结果是全进程的 moved-from 容器共享一个**可写**的全局空对象。
//
// **PkUseCount() 对 moved-from 返回的是哨兵的引用计数（1 + 进程内 moved-from
// 个数），不是 1。** 这是哨兵方案唯一的可观测代价，而 PkUseCount()/
// PkIsSharedWith() 按硬要求 4 只给单测用（真实调用点 isDetached()/isSharedWith()
// 实测 0 处），对任何容器语义零影响。**单测不要断言 moved-from 的计数**，
// 要断言真实语义（空、可写、写后能 detach、互不影响）。
//
// 唯一的异常代价：哨兵首次使用时那一次 make_shared 在 noexcept 函数里，
// OOM 会 terminate 而不是抛。Qt 的 shared_null 是静态对象、连这一次都没有；
// 取舍是接受它——OOM 时进程本来也活不下去。
//
// 五个特殊成员**全部显式写出**，因为：一旦用户声明了移动构造，隐式的拷贝构造
// 与拷贝赋值就会被定义为 **deleted**（[class.copy.ctor]/8）。而「拷贝必须 O(1)」
// 是 2286 处 Q_FOREACH 的命根子——漏写 `= default` 会把整条 CoW 通路静默拆掉，
// 编译期只报「拷贝构造被删除」这种离现场很远的错。单测里有 is_copy_constructible
// 的 static_assert 守着这条。
// ---------------------------------------------------------------------------

template <typename C>
class PkArrayData
{
    // 复用契约的机器化部分：换成注释里没写的 C 时，报错落在这里，
    // 而不是 make_shared 内部那一堆模板展开里。
    static_assert(std::is_default_constructible<C>::value,
                  "PkArrayData<C>：C 必须可默认构造（默认构造与共享空哨兵都要 make_shared<C>()）");
    static_assert(std::is_copy_constructible<C>::value,
                  "PkArrayData<C>：C 必须可拷贝构造（PkDetach() 的深拷贝是 make_shared<C>(*d)）");

public:
    PkArrayData() : d(std::make_shared<C>()) {}
    explicit PkArrayData(C init) : d(std::make_shared<C>(std::move(init))) {}

    // 拷贝 = 共享，O(1)，noexcept、零分配（shared_ptr 的拷贝构造/赋值都是 noexcept）。
    // 必须显式 = default：本类下面声明了移动构造，隐式拷贝就会被 deleted。
    ~PkArrayData() = default;
    PkArrayData(const PkArrayData &) = default;
    PkArrayData &operator=(const PkArrayData &) = default;

    // 移动：noexcept + 零分配。源换成共享空哨兵（机制与取舍见类头）。
    PkArrayData(PkArrayData &&o) noexcept : d(std::exchange(o.d, PkSharedEmpty())) {}

    PkArrayData &operator=(PkArrayData &&o) noexcept
    {
        // 自移动（a = std::move(a)）必须是 no-op：不加这道判断，下面会把自己的
        // 数据搬走再塞个空哨兵进来，内容凭空丢掉。Qt 的 QVector 移动赋值经由
        // 「构造临时量再 swap」也是 no-op，这里对齐同样的可观察结果。
        if (this != &o) {
            d = std::exchange(o.d, PkSharedEmpty());
        }
        return *this;
    }

    // 零分配、noexcept 的交换：只换 shared_ptr 指针本身。
    //
    // Task 2–6 实现 Qt 的 swap() 走它，别退回 std::swap(m_d, o.m_d)——那会展开成
    // 1 次移动构造 + 2 次移动赋值，每一次都要在**全进程共享**的空哨兵上做一对
    // 原子增减引用（3 次带原子的操作 vs 1 次纯指针交换；哨兵还是跨线程共享的
    // 缓存行，是天然的竞争点）。
    //
    // 注意口径：移动改用哨兵之后，退回 std::swap **不再多出堆分配**，差别只剩
    // 常数因子。单测 swapDoesNotAllocate 用一个「从不被移动、因此哨兵始终是冷的」
    // 探针类型把这条区分出来——冷哨兵下走移动路径会触发哨兵的首次 make_shared。
    //
    // （容器侧 `.swap(` 粗口径实测约 17 处——该数含 std::string::swap 等
    //  非容器调用，是上界，不是容器 swap 的精确调用点数。）
    void PkSwap(PkArrayData &o) noexcept { d.swap(o.d); }

    // 读路径：绝不 detach
    const C &PkConst() const noexcept { return *d; }

    // 写路径：每个非 const 方法进来第一件事就是调它
    C &PkMut() { PkDetach(); return *d; }

    // 引用计数 >1 时深拷贝。use_count()==1 时必须是零成本的
    void PkDetach()
    {
        if (d.use_count() > 1) {
            d = std::make_shared<C>(*d);
        }
    }

    // 供单测断言 COW 是否真的发生；不进 compat 垫片、不对调用点暴露。
    // 注意：对 moved-from 对象返回的是哨兵计数（1 + 进程内 moved-from 个数）。
    long PkUseCount() const noexcept { return d.use_count(); }
    bool PkIsSharedWith(const PkArrayData &o) const noexcept { return d == o.d; }

private:
    // 进程内共享的空 C。每个 C 一份（模板静态局部量），函数局部 static 保证
    // 线程安全的一次性初始化，也避开了静态初始化顺序问题。
    //
    // 它自己长期持有 1 份引用，这是「往哨兵写会 detach」的全部机制所在：
    // 任何持有它的容器 use_count() 都 ≥ 2。别把它改成非 static 或不持引用。
    static const std::shared_ptr<C> &PkSharedEmpty()
    {
        static const std::shared_ptr<C> s = std::make_shared<C>();
        return s;
    }

    std::shared_ptr<C> d;
};
