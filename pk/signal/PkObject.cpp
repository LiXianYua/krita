#include "PkObject.h"

PkObject::PkObject(PkObject* parent)
    : m_alive(std::make_shared<std::atomic<bool>>(true)),
      m_lifecycle(std::make_shared<LifecycleState>())
{
    if (parent) {
        std::lock_guard<std::mutex> childrenLock(parent->m_childrenMutex);
        parent->m_children.push_back(this);
        parent->m_childLifecycle.push_back(m_lifecycle);
        m_parent = parent;
    }
}

void PkObject::deleteLater()
{
    bool expected = false;
    if (!m_lifecycle->deleteScheduled.compare_exchange_strong(expected, true)) return;
    PkObject* object = this;
    auto alive = m_alive;
    auto lifecycle = m_lifecycle;
    PkThreadCallQueue::post(thread(), [object, alive, lifecycle] {
        std::lock_guard<std::recursive_mutex> claim(*lifecycle->claim);
        if (!lifecycle->destroying && alive->load()) {
            lifecycle->destroying = true;
            delete object;
        }
    });
}

PkObject::~PkObject()
{
    // The claim is per object.  A parent deletion and this object's deferred
    // callback serialize with each other, without blocking unrelated object
    // destructors or holding a process-wide lock across derived destructors.
    std::lock_guard<std::recursive_mutex> claim(*m_lifecycle->claim);
    m_lifecycle->destroying = true;
    // 1. 断开所有连接：把双方列表里条目的 state->alive 置 false 并清空。
    disconnectAllOutgoing();
    disconnectAllIncoming();

    // 2. 置存活标志为 false——所有观察它的 QPointer 立即 isNull()==true。
    m_alive->store(false);

    // 3. 删除子对象。**按创建顺序（FIFO）**：探针 1 实测 c1→c2→c3，不是 LIFO。
    //    子对象析构时会把自己从本对象的 m_children 里摘除（见步骤 4），因此
    //    m_children 在遍历中不断收缩——按索引遍历会跳过元素。改为每次取队首，
    //    删完后向量前移，天然按创建顺序（FIFO）逐个销毁，且循环终止。
    while (true) {
        PkObject* child = nullptr;
        std::shared_ptr<LifecycleState> childLifecycle;
        {
            std::lock_guard<std::mutex> childrenLock(m_childrenMutex);
            if (m_children.empty()) break;
            child = m_children.front();
            childLifecycle = m_childLifecycle.front();
            m_children.erase(m_children.begin());
            m_childLifecycle.erase(m_childLifecycle.begin());
        }
        std::lock_guard<std::recursive_mutex> childClaim(*childLifecycle->claim);
        if (!childLifecycle->destroying) {
            childLifecycle->destroying = true;
            delete child;
        }
    }

    // 4. 从 parent 的 children 里摘除自己。
    if (m_parent) {
        std::lock_guard<std::mutex> childrenLock(m_parent->m_childrenMutex);
        auto& sib = m_parent->m_children;
        for (auto it = sib.begin(); it != sib.end(); ++it) {
            if (*it == this) {
                const auto index = static_cast<std::size_t>(it - sib.begin());
                sib.erase(it);
                m_parent->m_childLifecycle.erase(m_parent->m_childLifecycle.begin() + index);
                break;
            }
        }
    }
}

void PkObject::appendConnection(PkObject* sender, PkObject* receiver,
                                PkMemberFnKey key,
                                std::shared_ptr<PkSlotBase> slot,
                                std::shared_ptr<PkConnectionState> state,
                                PkConnectionType type,
                                bool hasSlotKey,
                                PkMemberFnKey slotKey)
{
    // 同一逻辑条目进 sender->m_outgoing 与 receiver->m_incoming 两份，
    // 共享同一个 slot 盒（shared_ptr）与同一个 state（shared_ptr）。
    // 双方列表里的条目 state 相同 → 任何一侧置 alive=false，另一侧的 emit 立即跳过。
    ConnectionEntry entry{key, receiver, state, std::move(slot), type, hasSlotKey, slotKey};
    sender->m_outgoing.push_back(entry);
    receiver->m_incoming.push_back(entry);
}

bool PkObject::disconnect(PkConnection& connection)
{
    if (!connection.isValid()) return false;
    auto st = connection.state();
    if (!st->alive) return false;
    st->alive = false;                 // 置 dead：emit 遍历从此跳过它
    return true;
}

bool PkObject::disconnect(const PkObject* sender, std::nullptr_t,
                          const PkObject* receiver, std::nullptr_t)
{
    // 探针语义：只断「sender 的 m_outgoing 里 receiver 匹配」的活条目，
    // 别的 receiver 不受影响（探针 r4.a=0 r4b.a=1）。返回是否断到至少一条。
    PkObject* s = const_cast<PkObject*>(sender);
    PkObject* r = const_cast<PkObject*>(receiver);
    bool any = false;
    for (auto& e : s->m_outgoing) {
        if (e.state && e.state->alive && e.receiver == r) {
            e.state->alive = false;
            any = true;
        }
    }
    return any;
}

void PkObject::disconnectAllOutgoing()
{
    for (auto& e : m_outgoing) if (e.state) e.state->alive = false;
    m_outgoing.clear();
}

void PkObject::disconnectAllIncoming()
{
    for (auto& e : m_incoming) if (e.state) e.state->alive = false;
    m_incoming.clear();
}

std::vector<PkObject*>& PkObject::s_emitStack()
{
    static thread_local std::vector<PkObject*> stack;
    return stack;
}

PkObject* PkObject::sender()
{
    auto& stack = s_emitStack();
    return stack.empty() ? nullptr : stack.back();
}

PkCallLifetime PkObject::callLifetime() const
{
    // R-34 Task 4：把析构用的 claim 锁与存活标志一起交给投递侧。析构持同一
    // 把 claim（见 ~PkObject 顶部），投递执行侧在 claim 下重查 alive，两者
    // 串行化，关闭「对象先死 + 目标线程后 pump」的悬垂 UB。
    return { m_lifecycle->claim, m_alive };
}
