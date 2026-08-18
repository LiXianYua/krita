#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <tuple>
#include <utility>
#include "PkSignalTraits.h"

// Qt::ConnectionType 的替代。Auto/Direct/Unique 同步立即执行（Unique 的
// dispatch 与 Auto 等价，区别只在 connect 期去重）；Queued/BlockingQueued
// 自 R-24 Task 3 起真实投递到目标线程（PkObject::activateSignal 按类型
// 分派，走 pk/concurrent 的 PkThreadCallQueue），细节见
// PkObject.h::activateSignal 与 README.md 偏离清单第 1 条。
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

// ---- slot 装箱 helper（样板展开，非占位）----

// PkCallSlotPrefix：把接收到的完整信号参数（SignalArgs... = signal 的参数包）
// 只取前 sizeof...(SlotArgs) 个转给 slot —— Qt 语义「signal 参数可以多于 slot，
// slot 只用前 N 个」。tests/ 单测没压到这条：它是试接
// KisSignalAutoConnectionTest::testOverloadConnection 里
// sigTest2(const QString&,const QString&) → slotTest2(const QString&) 真实压出来的。
// 前缀取法：forward_as_tuple 绑成引用 tuple，再用 index_sequence 取前 N 项。
// 参数类型不一致（无法隐式转换）时编译报错——与「类型不匹配编译期炸」的形态一致。
template <typename Ret, typename Obj, typename... SlotArgs,
          std::size_t... Is, typename... SignalArgs>
void PkCallSlotPrefix(Ret (Obj::*slot)(SlotArgs...), Obj* obj,
                      std::index_sequence<Is...>, SignalArgs&&... args)
{
    std::tuple<SignalArgs&&...> tup(std::forward<SignalArgs>(args)...);
    (obj->*slot)(std::get<Is>(tup)...);
    (void)tup;   // slot 无参（Is 为空）时 tup 未读，消 -Wunused-but-set-variable
}

// PkMakeSlotFnFromTuple<Tuple>(slot, receiver) -> std::function<void(SignalArgs...)>：
// Tuple = std::tuple<SignalArgs...>（信号侧参数包，从 ArgsTuple 展开传入）。
// 把「成员函数指针调用」包进按信号签名收参的 lambda，槽只取前缀。
// receiver 是 const 指针需 const_cast——slot 是普通成员函数，可在非 const 对象
// 上调用；实际对象在 connect 端非 const。
template <typename Tuple>
struct PkMakeSlotFnFromTupleHelper;

template <typename... SignalArgs>
struct PkMakeSlotFnFromTupleHelper<std::tuple<SignalArgs...>> {
    template <typename Ret, typename Obj, typename... SlotArgs>
    static std::function<void(SignalArgs...)> make(Ret (Obj::*slot)(SlotArgs...),
                                                   const Obj* receiver)
    {
        static_assert(sizeof...(SlotArgs) <= sizeof...(SignalArgs),
                      "slot accepts more parameters than signal provides");
        return [receiver, slot](SignalArgs... args) {
            PkCallSlotPrefix(slot, const_cast<Obj*>(receiver),
                             std::make_index_sequence<sizeof...(SlotArgs)>{},
                             std::forward<SignalArgs>(args)...);
        };
    }
};

template <typename Tuple, typename Ret, typename Obj, typename... SlotArgs>
auto PkMakeSlotFnFromTuple(Ret (Obj::*slot)(SlotArgs...), const Obj* receiver)
{
    return PkMakeSlotFnFromTupleHelper<Tuple>::make(slot, receiver);
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
