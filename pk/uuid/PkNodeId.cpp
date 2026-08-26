// PkNodeId.cpp —— QUuid 的零 Qt 对应物实现。
//
// 解析/格式化对拍 Qt 5.15 QUuid：
//   - fromString/解析构造接受 `{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}` 与
//     `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`（无花括号），严格 8-4-4-4-12
//     十六进制 + 连字符。无效输入 → null（不抛异常）。
//   - toString() 输出带花括号小写十六进制。
//   - createUuid() 生成 RFC 4122 v4：随机 16 字节，置 version=4（byte6 高
//     4 位）、variant=10（byte8 高 2 位）。
#include "PkNodeId.h"

#include <cctype>
#include <cstdio>
#include <random>

namespace {

// 单个十六进制字符 → 值（0-15），非法返回 -1。
int hexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// 严格 8-4-4-4-12 解析。接受可选花括号。成功返回 true 并填充 out。
bool parseUuid(const std::string &s, std::array<std::uint8_t, 16> &out)
{
    const char *p = s.c_str();
    const char *end = p + s.size();

    if (end - p >= 2 && p[0] == '{' && end[-1] == '}') {
        ++p;
        --end;
    }

    static const int groupLen[5] = {8, 4, 4, 4, 12};
    std::size_t byteIdx = 0;
    for (int g = 0; g < 5; ++g) {
        if (g > 0) {
            if (p >= end || *p != '-') return false;
            ++p;
        }
        int nibbleHigh = -1; // 每个字节先收高 4 位再收低 4 位
        for (int i = 0; i < groupLen[g]; ++i) {
            if (p >= end) return false;
            int v = hexVal(*p);
            if (v < 0) return false;
            if (i % 2 == 0) {
                nibbleHigh = v;
            } else {
                if (byteIdx >= 16) return false;
                out[byteIdx++] = static_cast<std::uint8_t>((nibbleHigh << 4) | v);
            }
            ++p;
        }
        // 奇数长度的组（8 的倍数必然是偶数，这里组长度都是偶数）不会走到
        // 残留 nibble；防御性检查：
        if (byteIdx < 16 && g == 4 && (groupLen[g] % 2 == 1)) {
            return false;
        }
    }
    return byteIdx == 16 && p == end;
}

void formatUuid(const std::array<std::uint8_t, 16> &data, std::string &out)
{
    out.clear();
    out.reserve(38);
    out.push_back('{');
    char buf[3];
    for (int i = 0; i < 16; ++i) {
        std::snprintf(buf, sizeof(buf), "%02x", data[i]);
        out.append(buf);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            out.push_back('-');
        }
    }
    out.push_back('}');
}

} // namespace

PkNodeId::PkNodeId()
{
    m_data.fill(0);
}

PkNodeId::PkNodeId(const PkString &text)
{
    if (!parseUuid(text.PkToUtf8(), m_data)) {
        m_data.fill(0);
    }
}

PkNodeId PkNodeId::createUuid()
{
    static thread_local std::mt19937 rng{std::random_device{}()};
    PkNodeId result; // 全零
    for (auto &b : result.m_data) {
        b = static_cast<std::uint8_t>(rng());
    }
    result.m_data[6] = static_cast<std::uint8_t>((result.m_data[6] & 0x0f) | 0x40); // version 4
    result.m_data[8] = static_cast<std::uint8_t>((result.m_data[8] & 0x3f) | 0x80); // variant 10
    return result;
}

PkNodeId PkNodeId::fromString(const PkString &text)
{
    return PkNodeId(text);
}

bool PkNodeId::isNull() const
{
    for (auto b : m_data) {
        if (b != 0) return false;
    }
    return true;
}

PkString PkNodeId::toString() const
{
    std::string s;
    formatUuid(m_data, s);
    return PkString::PkFromUtf8(s.c_str(), static_cast<int>(s.size()));
}

bool PkNodeId::operator==(const PkNodeId &o) const
{
    return m_data == o.m_data;
}

bool PkNodeId::operator!=(const PkNodeId &o) const
{
    return !(*this == o);
}

bool PkNodeId::operator<(const PkNodeId &o) const
{
    return m_data < o.m_data;
}
