#include "PkConfigStore.h"

PkConfigStore &PkConfigStore::instance()
{
    static PkConfigStore store;   // C++11 magic static，线程安全
    return store;
}

PkString PkConfigStore::get(const PkString &group, const PkString &key, const PkString &fallback) const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
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
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_data[group][key] = value;
}

bool PkConfigStore::has(const PkString &group, const PkString &key) const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    auto groupIt = m_data.find(group);
    if (groupIt == m_data.end()) {
        return false;
    }
    return groupIt->second.find(key) != groupIt->second.end();
}

void PkConfigStore::remove(const PkString &group, const PkString &key)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    auto groupIt = m_data.find(group);
    if (groupIt == m_data.end()) {
        return;
    }
    groupIt->second.erase(key);
    if (groupIt->second.empty()) {
        m_data.erase(groupIt);
    }
}

void PkConfigStore::clearGroup(const PkString &group)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_data.erase(group);
}
