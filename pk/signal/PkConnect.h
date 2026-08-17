#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <tuple>
#include <utility>
#include "PkSignalTraits.h"

// Qt::ConnectionType 的替代。R-05 只实现同步直连语义：Auto/Direct 直接调用；
// Queued/BlockingQueued 在无事件循环的世界里退化为 Direct（跨线程投递归 Q-8，
// 见 plan「偏离声明」）。Unique 是「同信号同槽不重复连接」。
enum class PkConnectionType { Auto, Direct, Queued, BlockingQueued, Unique };

// QOverload<Args...>::of(ptr) —— 信号/槽重载消歧。Qt 里同名信号有多组参数时，
// `&C::sig` 是模糊的，必须 `QOverload<const QString&, const QString&>::of(&C::sig)`。
template <typename... Args>
struct QOverload {
    template <typename R, typename T>
    static constexpr auto of(R (T::*ptr)(Args...)) -> decltype(ptr) { return ptr; }
};

// 类型擦除的槽基类。连接条目里存派生类 PkSlotImpl<Args...>，emit 时 dynamic_cast
// 回具体类型调用。基类虚析构让条目销毁时正确释放 std::function，并满足 RTTI 多态。
struct PkSlotBase {
    virtual ~PkSlotBase() = default;
};

template <typename... Args>
struct PkSlotImpl : PkSlotBase {
    std::function<void(Args...)> fn;
    explicit PkSlotImpl(std::function<void(Args...)> f) : fn(std::move(f)) {}
};

// ---- 两个 helper（样板展开，非占位）----

// PkMakeSlotFn(slot, receiver) -> std::function<void(Args...)>：
// 把「成员函数指针调用」包进 lambda。receiver 是 const 指针需 const_cast——
// slot 是普通成员函数，可在非 const 对象上调用；实际对象在 connect 端非 const。
// Args... 从 slot 自身签名推导；信号侧参数通过 PkSlotImpl 的构造转换对齐。
template <typename Ret, typename Obj, typename... Args>
std::function<void(Args...)> PkMakeSlotFn(Ret (Obj::*slot)(Args...), const Obj* receiver)
{
    return [receiver, slot](Args&&... a) {
        (const_cast<Obj*>(receiver)->*slot)(std::forward<Args>(a)...);
    };
}

// PkSlotImplFromTuple<Tuple> = PkSlotImpl<展开后 Args...>：
// 把 PkSignalTraits<Func>::ArgsTuple（std::tuple<Args...>）还原成具体槽类型。
template <typename Tuple>
struct PkSlotImplFromTupleHelper;

template <typename... Args>
struct PkSlotImplFromTupleHelper<std::tuple<Args...>> {
    using type = PkSlotImpl<Args...>;
};

template <typename Tuple>
using PkSlotImplFromTuple = typename PkSlotImplFromTupleHelper<Tuple>::type;
