#pragma once
#include <memory>
#include <vector>
#include <atomic>
#include "PkConnection.h"

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

    // 连接 / 断开 / sender() 的声明在 Task 2 / Task 3 补。这里先给对象树需要的
    // 内部接口：信号发射（Task 2）与 QPointer 通知（Task 3）都经它走。

    // QPointer<T> 观察对象生命周期用的「存活标志」。PkObject 构造时分配，
    // 析构时置 false；QPointer 持有 weak 视图据此判断 isNull()。
    std::shared_ptr<std::atomic<bool>> aliveFlag() const { return m_alive; }

    // 析构时断开与 this 相关的全部连接（Task 2 填实现）。先声明，Task 2 实现。
    void disconnectAllOutgoing();
    void disconnectAllIncoming();

protected:
    // 信号发射入口（Task 2 实现）。信号定义（生成器生成）调用它。
    template <typename... Args>
    static void activateSignal(PkObject* sender, PkMemberFnKey key, Args&&... args);

private:
    // 对象树：parent 裸指针 + children 拥有。FIFO 析构顺序（探针 1：c1→c2→c3）。
    PkObject* m_parent = nullptr;
    std::vector<PkObject*> m_children;

    // QPointer 存活标志（析构置 false）。
    std::shared_ptr<std::atomic<bool>> m_alive;

    // 连接列表（Task 2 填类型）。前向声明避免本 Task 暴露连接条目细节。
    struct ConnectionEntry;
    std::vector<ConnectionEntry> m_outgoing;   // this 作为 sender
    std::vector<ConnectionEntry> m_incoming;   // this 作为 receiver
};
