#pragma once
#include "PkString.h"
#include <map>

// PkConfigStore —— 进程内单例，纯字符串二级 map（group -> key -> value）。
// 不知道值的类型，类型化的编解码在 PkConfigGroup 里做。不做真实磁盘持久化
// （Global Constraints，见 task brief）。
class PkConfigStore
{
public:
    static PkConfigStore &instance();

    PkString get(const PkString &group, const PkString &key, const PkString &fallback) const;
    void set(const PkString &group, const PkString &key, const PkString &value);
    bool has(const PkString &group, const PkString &key) const;
    void remove(const PkString &group, const PkString &key);

private:
    PkConfigStore() = default;
    std::map<PkString, std::map<PkString, PkString>> m_data;
};
