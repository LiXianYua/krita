#pragma once
#include <memory>
#include <utility>
#include "PkSignalTraits.h"

// QMetaObject::Connection 的替代。默认构造是「空句柄」（isValid()==false），
// connect() 返回的句柄内部持 PkConnectionState，disconnect(PkConnection&)
// 通过它找到并标记连接。比较按状态指针身份。
class PkConnection
{
public:
    PkConnection() = default;

    // 对齐 Qt post-disconnect 语义：断开后 isValid() 变 false（不只查「非空」，
    // 还查状态 alive）。disconnect 只置 alive=false，句柄仍持 state，故必须查 alive。
    bool isValid() const { return m_state && m_state->alive; }

    bool operator==(const PkConnection& o) const { return m_state == o.m_state; }
    bool operator!=(const PkConnection& o) const { return !(*this == o); }

private:
    friend class PkObject;
    explicit PkConnection(std::shared_ptr<PkConnectionState> s) : m_state(std::move(s)) {}
    std::shared_ptr<PkConnectionState> state() const { return m_state; }

    std::shared_ptr<PkConnectionState> m_state;
};

// PkMetaObject::Connection —— 连接句柄类型的容器名。S-02-a 迁移时 Krita 消费方
// （libs/global/kis_signal_auto_connection.h）把 QMetaObject::Connection 改写为
// PkMetaObject::Connection；compat/QObject 的 QMetaObject 替身只在 real Qt 缺席时
// 定义（real Qt 在场时让位给真 QMetaObject，语义不同），而 PkMetaObject 是 pk 自有
// 名字、与 real Qt 是否在场无关，故独立于 QMetaObject 定义。消费方经
// PkMetaObject::Connection 拿到的就是 PkConnection。
struct PkMetaObject {
    using Connection = PkConnection;
};
