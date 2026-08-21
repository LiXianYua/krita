/*
    SPDX-FileCopyrightText: 2026 S-03-a
    SPDX-License-Identifier: LGPL-2.1-or-later

    PkBitArray —— 位数组的零 Qt 垫片（S 线剥 Qt 用）。
    消费方：KoColorSpace::channelFlags、KoCompositeOp::ParameterInfo::channelFlags、
    compositeops/*（KoCompositeOpBase、KoOptimizedCompositeOp*128/32、Over32 等）。
    语义对齐 Qt 5.15：at()/testBit() 越界返回 false（Qt 是 assert，消费方在范围内
    用，宽松即可）；fill(bool, first, last) 的 last<0 表示 size()-1。
 */

#ifndef PK_BITARRAY_H
#define PK_BITARRAY_H

#include <cstdint>
#include <vector>

class PkBitArray
{
public:
    PkBitArray() = default;              // 空
    explicit PkBitArray(int size)        // size 个 false
        : m_bits(static_cast<size_t>(size < 0 ? 0 : size), 0)
    {
    }
    PkBitArray(int size, bool value)     // size 个 value
        : m_bits(static_cast<size_t>(size < 0 ? 0 : size), value ? 1 : 0)
    {
    }

    int size() const { return static_cast<int>(m_bits.size()); }
    int count() const { return size(); } // == size()
    bool isEmpty() const { return m_bits.empty(); }

    bool at(int i) const
    {
        return i >= 0 && i < size() && m_bits[static_cast<size_t>(i)] != 0;
    }
    bool testBit(int i) const { return at(i); }

    void setBit(int i)
    {
        if (i >= 0 && i < size()) {
            m_bits[static_cast<size_t>(i)] = 1;
        }
    }
    void setBit(int i, bool value)
    {
        if (i >= 0 && i < size()) {
            m_bits[static_cast<size_t>(i)] = value ? 1 : 0;
        }
    }
    void clearBit(int i)
    {
        if (i >= 0 && i < size()) {
            m_bits[static_cast<size_t>(i)] = 0;
        }
    }

    // Qt 位数组 fill(bool, int first, int last)：last<0 → size()-1。
    void fill(bool value, int first = 0, int last = -1)
    {
        if (size() == 0) {
            return;
        }
        if (first < 0) {
            first = 0;
        }
        if (last < 0 || last >= size()) {
            last = size() - 1;
        }
        for (int i = first; i <= last; ++i) {
            m_bits[static_cast<size_t>(i)] = value ? 1 : 0;
        }
    }

    void resize(int size)
    {
        if (size < 0) {
            size = 0;
        }
        m_bits.resize(static_cast<size_t>(size), 0);
    }

    bool operator==(const PkBitArray &other) const { return m_bits == other.m_bits; }
    bool operator!=(const PkBitArray &other) const { return !(*this == other); }

    // Qt 位数组的位运算符（KoColorSpace.cpp:567 用 `|` 合并 channelFlags）。
    // 结果长度取两者较长者，缺位按 false 处理。
    PkBitArray operator|(const PkBitArray &other) const
    {
        const int n = size() > other.size() ? size() : other.size();
        PkBitArray r(n);
        for (int i = 0; i < n; ++i) {
            r.m_bits[static_cast<size_t>(i)] = (at(i) || other.at(i)) ? 1 : 0;
        }
        return r;
    }
    PkBitArray operator&(const PkBitArray &other) const
    {
        const int n = size() > other.size() ? size() : other.size();
        PkBitArray r(n);
        for (int i = 0; i < n; ++i) {
            r.m_bits[static_cast<size_t>(i)] = (at(i) && other.at(i)) ? 1 : 0;
        }
        return r;
    }

private:
    std::vector<uint8_t> m_bits;   // 每字节 1 bit（0/1），下标即 bit 索引
};

#endif // PK_BITARRAY_H
