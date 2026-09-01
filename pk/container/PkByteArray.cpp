#include "PkByteArray.h"

#include <string>

PkByteArray::PkByteArray() = default;

PkByteArray::PkByteArray(const char* data, int len)
{
    if (len <= 0) return;
    m_data.reserve(static_cast<size_t>(len));
    for (int i = 0; i < len; ++i)
        m_data.push_back(static_cast<uint8_t>(data[i]));
}

PkByteArray::PkByteArray(const std::vector<uint8_t>& data) : m_data(data) {}

char* PkByteArray::data() { return reinterpret_cast<char*>(m_data.data()); }

const char* PkByteArray::data() const
{
    return m_data.empty() ? "" : reinterpret_cast<const char*>(m_data.data());
}

const char* PkByteArray::constData() const
{
    return m_data.empty() ? "" : reinterpret_cast<const char*>(m_data.data());
}

int PkByteArray::size() const { return static_cast<int>(m_data.size()); }
bool PkByteArray::isEmpty() const { return m_data.empty(); }

void PkByteArray::resize(int n)
{
    if (n <= 0) { m_data.clear(); return; }
    m_data.resize(static_cast<size_t>(n));
}

PkByteArray PkByteArray::number(int n, int base)
{
    if (base == 10) {
        std::string s = std::to_string(n);
        return PkByteArray(s.data(), static_cast<int>(s.size()));
    }
    // base==2/8/16（及任意非 10 进制）：把 int 当 uint32 打全位补码。
    return number(static_cast<unsigned int>(n), base);
}

PkByteArray PkByteArray::number(unsigned int n, int base)
{
    if (base == 10) {
        std::string s = std::to_string(n);
        return PkByteArray(s.data(), static_cast<int>(s.size()));
    }
    if (base < 2) base = 2;
    if (base > 36) base = 36;
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char buf[40];
    char* p = buf + sizeof(buf);
    unsigned int u = n;
    const unsigned int ub = static_cast<unsigned int>(base);
    do {
        *--p = digits[u % ub];
        u /= ub;
    } while (u > 0);
    return PkByteArray(p, static_cast<int>(buf + sizeof(buf) - p));
}

bool PkByteArray::operator==(const PkByteArray& other) const { return m_data == other.m_data; }
bool PkByteArray::operator!=(const PkByteArray& other) const { return !(*this == other); }
