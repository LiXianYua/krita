#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <tuple>
#include <utility>
#include "PkSignalTraits.h"

// Qt::ConnectionType 的替代（final whole-branch review I-2 更正：此前这段
// 注释说 Auto/Unique 一律同步立即执行，与 activateSignal 实际行为相反，
// 已按真实行为改写）。真实行为：
// - Direct：永远同步立即执行。
// - Auto/Unique：视 emit 时 sender/receiver 是否同线程决定——同线程同步
//   立即执行，跨线程则投递（Unique 的 dispatch 与 Auto 完全等价，区别只在
//   connect 期是否去重）。
// - Queued/BlockingQueued：永远投递，即使 sender/receiver 同线程也不例外
//   （自 R-24 Task 3 起真实投递，不再退化为 Direct）。
// 投递均走 pk/concurrent 的 PkThreadCallQueue（PkObject::activateSignal 按
// 类型分派），细节见 PkObject.h::activateSignal 与 README.md 偏离清单第 1 条。
// PkConnectionType 位值对齐真 Qt 5.15.7 qnamespace.h:1337-1343：
// Auto=0 Direct=1 Queued=2 BlockingQueued=3 Unique=0x80（Unique 是 flag 位，
// 不是序号 4）。R-36 修正：此前 Unique 隐式 =4，与 Qt 位值不一致；pk/signal
// 内部只做 `==` 比较（PkObject.h:181/296/302/326），改位值无行为影响，但
// 让 `int(Qt::UniqueConnection)`（经 compat 别名）对齐真 Qt。
enum class PkConnectionType { Auto, Direct, Queued, BlockingQueued, Unique = 0x80 };

// QOverload<Args...>::of(ptr) —— 信号/槽重载消歧。Qt 里同名信号有多组参数时，
// `&C::sig` 是模糊的，必须 `QOverload<const QString&, const QString&>::of(&C::sig)`。
// 让位守卫：真 Qt 的 qglobal.h 也定义 QOverload（同名同用途），real Qt 已进 TU
// （QT_CORE_LIB）时让位，否则与真 Qt 头重定义（libs/global 基线测试的场景）。
#if !defined(QT_CORE_LIB)
template <typename... Args>
struct QOverload {
    template <typename R, typename T>
    static constexpr auto of(R (T::*ptr)(Args...)) -> decltype(ptr) { return ptr; }
};
#endif

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
