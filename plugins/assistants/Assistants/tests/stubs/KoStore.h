/* Task-local faithful in-memory KoStore surface for assistant XML tests. */
#pragma once

#include <PkAuxTypes.h>
#include <PkString.h>

#include <algorithm>

class KoStore
{
public:
    KoStore() = default;
    explicit KoStore(const PkByteArray &data)
        : m_data(data)
    {
    }

    bool open(const PkString &path)
    {
        m_open = !path.isEmpty();
        return m_open;
    }
    bool close()
    {
        const bool wasOpen = m_open;
        m_open = false;
        return wasOpen;
    }
    long long size() const { return m_data.size(); }
    PkByteArray read(long long maximum) const
    {
        const int length = std::max(0, std::min(m_data.size(), static_cast<int>(maximum)));
        return PkByteArray(m_data.constData(), length);
    }

private:
    PkByteArray m_data;
    bool m_open = false;
};
