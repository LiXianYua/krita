#include "PkConfigStore.h"

PkConfigStore &PkConfigStore::instance()
{
    static PkConfigStore store;   // C++11 magic static，线程安全
    return store;
}

PkString PkConfigStore::get(const PkString &group, const PkString &key, const PkString &fallback) const
{
    auto groupIt = m_data.find(group);
    if (groupIt == m_data.end()) {
        return fallback;
    }
    auto keyIt = groupIt->second.find(key);
    if (keyIt == groupIt->second.end()) {
        return fallback;
    }
    return keyIt->second;
}

void PkConfigStore::set(const PkString &group, const PkString &key, const PkString &value)
{
    m_data[group][key] = value;
}

bool PkConfigStore::has(const PkString &group, const PkString &key) const
{
    auto groupIt = m_data.find(group);
    if (groupIt == m_data.end()) {
        return false;
    }
    return groupIt->second.find(key) != groupIt->second.end();
}

void PkConfigStore::remove(const PkString &group, const PkString &key)
{
    auto groupIt = m_data.find(group);
    if (groupIt == m_data.end()) {
        return;
    }
    groupIt->second.erase(key);
}
