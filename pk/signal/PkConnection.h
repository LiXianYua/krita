#pragma once
#include <memory>
#include "PkSignalTraits.h"

// QMetaObject::Connection 的替代。默认构造是「空句柄」（isValid()==false），
// connect() 返回的句柄内部持 PkConnectionState，disconnect(PkConnection&)
// 通过它找到并标记连接。比较按状态指针身份。
class PkConnection
{
public:
    PkConnection() = default;

    bool isValid() const { return static_cast<bool>(m_state); }

    bool operator==(const PkConnection& o) const { return m_state == o.m_state; }
    bool operator!=(const PkConnection& o) const { return !(*this == o); }

private:
    friend class PkObject;
    explicit PkConnection(std::shared_ptr<PkConnectionState> s) : m_state(std::move(s)) {}
    std::shared_ptr<PkConnectionState> state() const { return m_state; }

    std::shared_ptr<PkConnectionState> m_state;
};
