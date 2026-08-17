#include "PkObject.h"

// ConnectionEntry 是私有实现细节：头文件只前向声明，避免本 Task 暴露连接条目
// 形态。Task 2 在这里填真实定义；当前为空结构体（无成员），使 std::vector 的
// 析构能在编译期实例化——不完整类型会让 vector 析构报错。
struct PkObject::ConnectionEntry {};

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
    // 1. 断开所有连接（Task 2 实现真正内容前为空循环）。
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

void PkObject::disconnectAllOutgoing() {}
void PkObject::disconnectAllIncoming() {}
