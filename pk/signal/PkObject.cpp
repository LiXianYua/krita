#include "PkObject.h"

PkObject::PkObject(PkObject* parent)
    : m_alive(std::make_shared<std::atomic<bool>>(true))
{
    if (parent) {
        parent->m_children.push_back(this);
        m_parent = parent;
    }
}

PkObject::~PkObject()
{
    // 1. 断开所有连接：把双方列表里条目的 state->alive 置 false 并清空。
    disconnectAllOutgoing();
    disconnectAllIncoming();

    // 2. 置存活标志为 false——所有观察它的 QPointer 立即 isNull()==true。
    m_alive->store(false);

    // 3. 删除子对象。**按创建顺序（FIFO）**：探针 1 实测 c1→c2→c3，不是 LIFO。
    //    子对象析构时会把自己从本对象的 m_children 里摘除（见步骤 4），因此
    //    m_children 在遍历中不断收缩——按索引遍历会跳过元素。改为每次取队首，
    //    删完后向量前移，天然按创建顺序（FIFO）逐个销毁，且循环终止。
    while (!m_children.empty()) {
        delete m_children[0];
    }
    m_children.clear();

    // 4. 从 parent 的 children 里摘除自己。
    if (m_parent) {
        auto& sib = m_parent->m_children;
        for (auto it = sib.begin(); it != sib.end(); ++it) {
            if (*it == this) { sib.erase(it); break; }
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
