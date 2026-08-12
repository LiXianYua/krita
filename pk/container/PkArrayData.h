#pragma once

#include <memory>
#include <utility>

// ---------------------------------------------------------------------------
// PkArrayData<C> —— COW（写时复制）地基。C 是内层标准容器
// （std::vector<T> / std::map<K,V> / std::unordered_map<K,V> / ...）。
//
// 名字一律 Pk 前缀：它不是用量表里的公开 API，只是地基（与 pk/string/PkStringData
// 同惯例）。**只住在 pk/container/**，不与 pk/string/ 共用。
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
// 移动之后的源对象：**空容器，且完全可用**——PkConst()/PkMut()/PkDetach()/
// PkUseCount() 全都能照常调，源就是一个独占的空 C。这是 Qt 的语义
// （QVector 移动走之后 d 指向 Data::sharedNull()，仍是可用的空容器），线级 spec
// 的口径是「与 Qt 的任何行为差异默认都是缺陷」，所以地基必须兜住这条。
//
// **不要退回隐式移动**：隐式移动只会把 shared_ptr 搬空，源的 d 变成 nullptr，
// 之后任何 PkConst()/PkMut() 都是解空指针。把这件事推给 Task 2–6 各自处理，
// 等于同一份逻辑重写 6 遍、6 个出错点；兜在这里，上层的容器可以放心写
// `PkVector(PkVector &&) = default;`。
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
public:
    PkArrayData() : d(std::make_shared<C>()) {}
    explicit PkArrayData(C init) : d(std::make_shared<C>(std::move(init))) {}

    // 拷贝 = 共享，O(1)，noexcept（shared_ptr 的拷贝构造/赋值都是 noexcept）。
    // 必须显式 = default：本类下面声明了移动构造，隐式拷贝就会被 deleted。
    ~PkArrayData() = default;
    PkArrayData(const PkArrayData &) = default;
    PkArrayData &operator=(const PkArrayData &) = default;

    // 移动：**不是** noexcept，理由见类头。O(1)——一次小分配，零元素拷贝。
    //
    // 先把新的空 C 分配进自己的 d，再与源交换：源拿到那个空的，自己拿到源的数据。
    // 这个顺序是异常安全的——make_shared 抛了就什么都还没动，源保持原样。
    PkArrayData(PkArrayData &&o) : d(std::make_shared<C>()) { d.swap(o.d); }

    PkArrayData &operator=(PkArrayData &&o)
    {
        // 自移动（a = std::move(a)）必须是 no-op：不加这道判断，下面会把自己的
        // 数据搬走再塞个空的进来，内容凭空丢掉。Qt 的 QVector 移动赋值经由
        // 「构造临时量再 swap」也是 no-op，这里对齐同样的可观察结果。
        if (this != &o) {
            auto fresh = std::make_shared<C>();  // 先分配：抛了则两边都没变
            d = std::move(o.d);
            o.d = std::move(fresh);
        }
        return *this;
    }

    // 零分配、noexcept 的交换。Task 2–6 实现 Qt 的 swap()（容器侧实测 17 处）
    // 必须走它：写 std::swap(m_d, o.m_d) 会展开成 1 次移动构造 + 2 次移动赋值
    // = 3 次分配，而这里是 0 次。
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

    // 供单测断言 COW 是否真的发生；不进 compat 垫片、不对调用点暴露
    long PkUseCount() const noexcept { return d.use_count(); }
    bool PkIsSharedWith(const PkArrayData &o) const noexcept { return d == o.d; }

private:
    std::shared_ptr<C> d;
};
