#pragma once
#include <memory>
#include <vector>
#include <atomic>
#include <type_traits>
#include "PkConnection.h"
#include "PkConnect.h"

// QObject 的替代：父子树 + 生命周期 + （Task 2 起）信号连接。
// 无元对象、无字符串表、无属性系统——那三样在 Q-1 §6.1 的用量表里都是零或
// 已裁决删除（Q_PROPERTY 随 Q-9 整层删除）。
class PkObject
{
public:
    explicit PkObject(PkObject* parent = nullptr);
    virtual ~PkObject();

    PkObject(const PkObject&) = delete;
    PkObject& operator=(const PkObject&) = delete;

    PkObject* parent() const { return m_parent; }
    const std::vector<PkObject*>& children() const { return m_children; }

    // ---- 连接（同步直连）----
    // 成员函数指针 → 成员函数指针
    template <typename Func1, typename Func2>
    static PkConnection connect(
        const typename PkSignalTraits<Func1>::Object* sender, Func1 signal,
        const typename PkSignalTraits<Func2>::Object* receiver, Func2 slot,
        PkConnectionType type = PkConnectionType::Auto);

    // 成员函数指针 → lambda（receiver 仅用于生命周期绑定）
    template <typename Func1, typename Lambda>
    static PkConnection connect(
        const typename PkSignalTraits<Func1>::Object* sender, Func1 signal,
        const PkObject* receiver, Lambda&& lambda,
        PkConnectionType type = PkConnectionType::Auto);

    // 句柄式断开
    static bool disconnect(PkConnection& connection);

    // sender()（Task 3 实现，这里先声明）
    static PkObject* sender();

    // QPointer<T> 观察对象生命周期用的「存活标志」。PkObject 构造时分配，
    // 析构时置 false；QPointer 持有 weak 视图据此判断 isNull()。
    std::shared_ptr<std::atomic<bool>> aliveFlag() const { return m_alive; }

    // 析构时断开与 this 相关的全部连接。
    void disconnectAllOutgoing();
    void disconnectAllIncoming();

protected:
    // 信号发射入口。信号定义（生成器生成）调用它。
    // 按值收参（对齐 Qt QMetaObject::activate）：信号方法把参数原样传出（lvalue），
    // 显式 <Args...> 指定时用转发引用会变成 Args&& 而绑不上 lvalue，且推导引用会
    // 破坏 dynamic_cast<PkSlotImpl<Args...>*> 的类型匹配。
    template <typename... Args>
    static void activateSignal(PkObject* sender, PkMemberFnKey key, Args... args);

private:
    // 连接条目。类型擦除的槽 + 双方对象 + 状态 + 信号 key + 槽身份。
    struct ConnectionEntry {
        PkMemberFnKey key;
        PkObject* receiver = nullptr;          // 裸指针：仅用于 receiver 析构时断开，
                                               // 绝不在 receiver 析构后解引用
        std::shared_ptr<PkConnectionState> state;
        std::shared_ptr<PkSlotBase> slot;
        PkConnectionType type = PkConnectionType::Auto;
        bool hasSlotKey = false;               // 槽身份：成员函数指针槽有稳定 key；
                                               // lambda 槽无身份（false），Unique 不去重
        PkMemberFnKey slotKey;                 // hasSlotKey==true 时有效
    };

    // connect 的实现入口（非模板 helper，承接两个模板里公共的「塞两份列表」逻辑）。
    static void appendConnection(PkObject* sender, PkObject* receiver,
                                 PkMemberFnKey key,
                                 std::shared_ptr<PkSlotBase> slot,
                                 std::shared_ptr<PkConnectionState> state,
                                 PkConnectionType type,
                                 bool hasSlotKey,
                                 PkMemberFnKey slotKey);

    // emit 栈：activateSignal 进入时 push sender、离开时 RAII 守卫 pop；sender() 读栈顶。
    // thread_local——每线程独立发射栈，嵌套 emit 时返回最内层 sender。
    static std::vector<PkObject*>& s_emitStack();

    // 对象树：parent 裸指针 + children 拥有。FIFO 析构顺序（探针 1：c1→c2→c3）。
    PkObject* m_parent = nullptr;
    std::vector<PkObject*> m_children;

    // QPointer 存活标志（析构置 false）。
    std::shared_ptr<std::atomic<bool>> m_alive;

    // 连接列表：条目由 sender 和 receiver 双方各持一份（同一个 shared_ptr state 关联）。
    std::vector<ConnectionEntry> m_outgoing;   // this 作为 sender
    std::vector<ConnectionEntry> m_incoming;   // this 作为 receiver
};

// ---- 模板定义（成员模板只能放头文件）----

template <typename Func1, typename Func2>
PkConnection PkObject::connect(
    const typename PkSignalTraits<Func1>::Object* sender, Func1 signal,
    const typename PkSignalTraits<Func2>::Object* receiver, Func2 slot,
    PkConnectionType type)
{
    using Obj1 = typename PkSignalTraits<Func1>::Object;
    using Obj2 = typename PkSignalTraits<Func2>::Object;
    static_assert(std::is_base_of<PkObject, Obj1>::value, "sender must derive PkObject");
    static_assert(std::is_base_of<PkObject, Obj2>::value, "receiver must derive PkObject");

    PkMemberFnKey key = PkMemberFnKey::from(signal);
    PkMemberFnKey slotKey = PkMemberFnKey::from(slot);

    PkObject* s = const_cast<PkObject*>(static_cast<const PkObject*>(sender));
    PkObject* r = const_cast<PkObject*>(static_cast<const PkObject*>(receiver));

    // Unique：同 (信号 key, receiver, 槽 key) 三元组的活连接已存在 → 去重，返回无效句柄，
    // 不建立新连接（Qt::UniqueConnection 语义：same-quadruple 第二次 connect 返回空）。
    if (type == PkConnectionType::Unique) {
        for (const auto& e : s->m_outgoing) {
            if (e.state && e.state->alive && e.key == key &&
                e.receiver == r && e.hasSlotKey && e.slotKey == slotKey) {
                return PkConnection{};
            }
        }
    }

    auto state = std::make_shared<PkConnectionState>();

    // 把「成员函数调用」包进 std::function<void(Args...)>。
    using ArgsTuple = typename PkSignalTraits<Func1>::ArgsTuple;
    auto slotFn = PkMakeSlotFn(slot, receiver);

    auto slotBox = std::make_shared<PkSlotImplFromTuple<ArgsTuple>>(std::move(slotFn));

    appendConnection(s, r, key, slotBox, state, type, true, slotKey);
    return PkConnection(std::move(state));
}

template <typename Func1, typename Lambda>
PkConnection PkObject::connect(
    const typename PkSignalTraits<Func1>::Object* sender, Func1 signal,
    const PkObject* receiver, Lambda&& lambda,
    PkConnectionType type)
{
    using Obj1 = typename PkSignalTraits<Func1>::Object;
    static_assert(std::is_base_of<PkObject, Obj1>::value, "sender must derive PkObject");

    // lambda 槽无稳定身份：Unique 不去重，照常建立连接（hasSlotKey=false）。
    auto state = std::make_shared<PkConnectionState>();
    PkMemberFnKey key = PkMemberFnKey::from(signal);

    using ArgsTuple = typename PkSignalTraits<Func1>::ArgsTuple;
    auto slotBox = std::make_shared<PkSlotImplFromTuple<ArgsTuple>>(std::forward<Lambda>(lambda));

    appendConnection(
        const_cast<PkObject*>(static_cast<const PkObject*>(sender)),
        const_cast<PkObject*>(receiver),
        key, slotBox, state, type, false, PkMemberFnKey{});
    return PkConnection(std::move(state));
}

template <typename... Args>
void PkObject::activateSignal(PkObject* sender, PkMemberFnKey key, Args... args)
{
    // 遍历 sender 的 outgoing 里 key 匹配、state alive 的连接。
    // emit 中 disconnect 安全：disconnect 只把 state->alive 置 false，activateSignal
    // 遍历时跳过 dead，因此当前 emit 的其余连接不受迭代失效影响。
    // 按值把 args 传给每个槽（fn 签名按值收参，每槽各得一份拷贝）。

    // Task 3：emit 栈（thread_local）。进入时 push sender、离开时 RAII 守卫 pop，
    // sender() 读栈顶（嵌套 emit 返回最内层）。守卫保证槽抛异常时栈不残留，
    // 因此这里不裸 push/pop。
    struct EmitGuard {
        ~EmitGuard() { PkObject::s_emitStack().pop_back(); }
    };
    PkObject::s_emitStack().push_back(sender);
    EmitGuard guard;

    for (auto& e : sender->m_outgoing) {
        if (e.key == key && e.state && e.state->alive) {
            auto* impl = dynamic_cast<PkSlotImpl<Args...>*>(e.slot.get());
            if (impl) impl->fn(args...);
        }
    }
}
