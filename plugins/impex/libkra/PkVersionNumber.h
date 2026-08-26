// PkVersionNumber —— PkVersionNumber 的零 Qt 对应物（libkra 锁内新建）。
//
// PkVersionNumber 全树只有 libkra 3 文件用（实测 2026-08-25），故锁内垫片，
// 不进 pk 层。API 对拍 Qt 5.15 PkVersionNumber 的 libkra 实际用量：
//   - PkVersionNumber()                  空（isNull()==true）
//   - PkVersionNumber(int maj, int min)
//   - PkVersionNumber(int maj, int min, int patch)
//   - PkVersionNumber::fromString(const PkString&)   "1.2.3" 解析，非法 → 空
//   - isNull()                           空版本
//   - majorVersion()/minorVersion()/patchVersion()   缺段按 0
//   - operator<(const PkVersionNumber&)  段序比较（缺段按 0）
//
// header-only（类内定义隐式 inline），libkra 的 CMakeLists 无需加源文件。
#pragma once

#include "PkString.h"

#include <vector>

class PkVersionNumber
{
public:
    PkVersionNumber() = default;

    PkVersionNumber(int maj, int min)
    {
        m_segments.push_back(maj);
        m_segments.push_back(min);
    }

    PkVersionNumber(int maj, int min, int patch)
    {
        m_segments.push_back(maj);
        m_segments.push_back(min);
        m_segments.push_back(patch);
    }

    static PkVersionNumber fromString(const PkString &s)
    {
        // 对拍 PkVersionNumber::fromString：按 '.' 切段，每段十进制数；
        // 空段/非法段截断（其后内容忽略）。全非法 → 空版本。
        PkVersionNumber result;
        const std::string utf8 = s.PkToUtf8();
        std::string cur;
        auto flush = [&result](std::string &seg) {
            if (seg.empty()) return false;
            int value = 0;
            for (char c : seg) {
                if (c < '0' || c > '9') return false;
                value = value * 10 + (c - '0');
            }
            result.m_segments.push_back(value);
            return true;
        };
        for (char c : utf8) {
            if (c == '.') {
                if (!flush(cur)) return result; // 非法段 → 截断
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        flush(cur);
        return result;
    }

    bool isNull() const { return m_segments.empty(); }
    int majorVersion() const { return m_segments.size() > 0 ? m_segments[0] : 0; }
    int minorVersion() const { return m_segments.size() > 1 ? m_segments[1] : 0; }
    int patchVersion() const { return m_segments.size() > 2 ? m_segments[2] : 0; }

    bool operator<(const PkVersionNumber &o) const
    {
        const std::size_t n = m_segments.size() < o.m_segments.size()
                                  ? o.m_segments.size()
                                  : m_segments.size();
        for (std::size_t i = 0; i < n; ++i) {
            const int a = i < m_segments.size() ? m_segments[i] : 0;
            const int b = i < o.m_segments.size() ? o.m_segments[i] : 0;
            if (a != b) return a < b;
        }
        return false;
    }

private:
    std::vector<int> m_segments;
};
