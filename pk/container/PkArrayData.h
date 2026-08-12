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
// 移动之后的源对象：拷贝/移动/析构都用编译器隐式生成的版本（本类只有一个
// shared_ptr 成员）。因此**移动走之后 d 为空**，源对象处于「可析构、可再赋值」
// 的有效状态——PkUseCount()（返回 0）与 PkIsSharedWith() 仍可安全调用，但
// PkConst()/PkMut()/PkDetach() 不在承诺之内。Task 2–6 若要让自己的容器在被移动
// 之后仍满足 Qt 的「moved-from 是空容器且可用」语义，需要在**容器那一层**自己
// 定义移动操作，不要指望地基替你兜。
// ---------------------------------------------------------------------------

template <typename C>
class PkArrayData
{
public:
    PkArrayData() : d(std::make_shared<C>()) {}
    explicit PkArrayData(C init) : d(std::make_shared<C>(std::move(init))) {}

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
