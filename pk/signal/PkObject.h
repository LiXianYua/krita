#pragma once
#include <memory>
#include <vector>
#include <atomic>
#include <type_traits>
#include <utility>
#include "PkConnection.h"
#include "PkConnect.h"
#include "../concurrent/PkThread.h"
#include "../concurrent/PkThreadCallQueue.h"

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

    // 线程亲和性：QObject::thread()/moveToThread() 的替代。默认等于构造该
    // 对象的线程；moveToThread() 只改写这个标记（纯簿记，不触发任何唤醒/
    // 通知动作）——24 处真实调用点实测全部只是"确保后续排队投递落在正确
    // 线程"，从不依赖"moveToThread 之后有什么东西立刻开始跑"。
    // 写侧线程安全约束与 Qt 一致：只应该从该对象*当前*所在的线程调用
    // moveToThread()（Qt 官方文档明确要求）——真实调用点全部满足这条
    // （`this->moveToThread(...)`/`obj->moveToThread(...)` 都是在 obj 自己
    // 的构造线程或已知安全的时机调用）。
    // 读侧（final whole-branch review I-1，TSan 实测报出）：即使写侧 100%
    // 遵守上面这条约束，`activateSignal` 仍然会在*发射*线程上读
    // `e.receiver->thread()`，而 `moveToThread()` 可能同时在*receiver 自己*
    // 的线程上写同一个字段——这是两个不同线程对同一非原子字段的并发读写，
    // 与写侧契约是否被遵守无关。`m_thread` 因此是 `std::atomic<PkThreadId>`
    // （`sizeof(std::thread::id)==8` 且 lock-free，改动成本极小）：`thread()`/
    // `moveToThread()` 的调用方语义不变，只是读写现在真正同步。
    PkThreadId thread() const { return m_thread.load(); }
    void moveToThread(PkThreadId id) { m_thread.store(id); }

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

    // 4 参函数指针式断开：按 (信号 key, receiver, 槽 key) 三元素断「同信号同槽」。
    template <typename Func1, typename Func2>
    static bool disconnect(
        const typename PkSignalTraits<Func1>::Object* sender, Func1 signal,
        const typename PkSignalTraits<Func2>::Object* receiver, Func2 slot);

    // 断开全部式：断 sender 到 receiver 的**所有**活连接（不区分信号/槽）。
    // 用 std::nullptr_t 形参承接裸 0 / nullptr（Krita 拼写 disconnect(this, 0, this, 0)）。
    static bool disconnect(const PkObject* sender, std::nullptr_t,
                           const PkObject* receiver, std::nullptr_t);

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
        PkObject* receiver = nullptr;          // 裸指针：用于 receiver 析构断连、Unique 去重、
                                               // 断开全部式 disconnect 的匹配；绝不在
                                               // receiver 析构后解引用
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

    // 线程亲和性标记，构造时初始化为当前线程。原子类型见上方 thread()/
    // moveToThread() 注释（I-1：跨线程读写的数据竞争修复）。
    std::atomic<PkThreadId> m_thread{PkThread::currentThreadId()};

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

    // 把「成员函数调用」包进按信号签名收参的 std::function<void(Args...)>。
    // ArgsTuple 是信号侧参数包；PkMakeSlotFnFromTuple 让槽只取前缀（Qt 语义）。
    using ArgsTuple = typename PkSignalTraits<Func1>::ArgsTuple;
    auto slotFn = PkMakeSlotFnFromTuple<ArgsTuple>(slot, receiver);

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

template <typename Func1, typename Func2>
bool PkObject::disconnect(
    const typename PkSignalTraits<Func1>::Object* sender, Func1 signal,
    const typename PkSignalTraits<Func2>::Object* receiver, Func2 slot)
{
    using Obj1 = typename PkSignalTraits<Func1>::Object;
    using Obj2 = typename PkSignalTraits<Func2>::Object;
    static_assert(std::is_base_of<PkObject, Obj1>::value, "sender must derive PkObject");
    static_assert(std::is_base_of<PkObject, Obj2>::value, "receiver must derive PkObject");

    PkMemberFnKey key = PkMemberFnKey::from(signal);
    PkMemberFnKey slotKey = PkMemberFnKey::from(slot);

    PkObject* s = const_cast<PkObject*>(static_cast<const PkObject*>(sender));
    PkObject* r = const_cast<PkObject*>(static_cast<const PkObject*>(receiver));

    // 探针语义：按 (信号 key, receiver, 槽 key) 三元素在 sender 的 m_outgoing 里找活条目，
    // 找到置 dead 并返回 true，找不到返回 false。只断开**一个**匹配条目（与句柄式同为
    // 「断开一条连接」；Qt 同四元组重复 connect 后 disconnect 一次也只断一条）。
    for (auto& e : s->m_outgoing) {
        if (e.state && e.state->alive && e.key == key &&
            e.receiver == r && e.hasSlotKey && e.slotKey == slotKey) {
            e.state->alive = false;
            return true;
        }
    }
    return false;
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

    const PkThreadId callerThread = PkThread::currentThreadId();

    for (auto& e : sender->m_outgoing) {
        if (!(e.key == key) || !e.state || !e.state->alive) continue;
        auto* impl = dynamic_cast<PkSlotImpl<Args...>*>(e.slot.get());
        if (!impl) continue;

        // Unique 单独使用时（保留范围内实测无 Unique|其他类型的按位或组合）
        // 与 Auto 的 dispatch 行为等价：连接建立期已经去重，emit 期只需要
        // 按线程判断 Direct 还是 Queued。
        PkConnectionType effectiveType = e.type;
        if (effectiveType == PkConnectionType::Auto || effectiveType == PkConnectionType::Unique) {
            effectiveType = (e.receiver->thread() == callerThread)
                ? PkConnectionType::Direct
                : PkConnectionType::Queued;
        }

        if (effectiveType == PkConnectionType::Direct) {
            impl->fn(args...);
            continue;
        }

        // Queued / BlockingQueued：按值捕获 slot（续命）+ state（执行前
        // 重查 alive，投递期间可能被 disconnect）+ args。
        // M-2 更正：真正的拷贝来自这个 lambda 的按值捕获 `[..., args...]`，
        // 不是"args 反正已经是传给 activateSignal 的拷贝"（那个理由是错
        // 的——生成器产物调用 activateSignal 时的实参常是引用，且即便
        // activateSignal 自己的形参是按值局部变量，它们也会在 activateSignal
        // 返回时随栈帧一起失效；真正让参数活过 activateSignal 这次调用、
        // 撑到目标线程执行的，是这里 lambda 闭包对象里按值捕获出的独立
        // 副本）。排队路径因此要求参数类型可拷贝；拷贝在发射线程构造、在
        // 目标线程执行完后析构，涉及隐式共享类型时要留意别把这里"优化"成
        // 按引用捕获——那会让闭包捏着已经失效的引用。
        auto slotHolder = e.slot;
        auto state = e.state;
        const PkThreadId target = e.receiver->thread();
        auto call = [slotHolder, state, args...]() {
            if (!state->alive) return;   // 排队期间已断开，静默丢弃（对齐 Qt）
            auto* impl2 = static_cast<PkSlotImpl<Args...>*>(slotHolder.get());
            impl2->fn(args...);
        };
        if (effectiveType == PkConnectionType::BlockingQueued) {
            PkThreadCallQueue::postBlocking(target, std::move(call));
        } else {
            PkThreadCallQueue::post(target, std::move(call));
        }
    }
}
