/*
 *  SPDX-FileCopyrightText: 2026 paint_tips migration
 *
 *  flake 迁移到 PkString 时，QString 用量表外的成员方法（toLatin1 / indexOf /
 *  endsWith / chop 等）在 PkString 上不存在——PkString 的公开 API 刻意收窄到
 *  QString 14 项用量表（见 pk/string/PkString.h 注释）。
 *
 *  既定惯例（pk/port/PkResourceStorage.cpp:16 注释、libs/psdutils/psd_utils.h
 *  的 psdToLatin1）是「改写调用点、不扩 PkString」。本头集中 flake 需要的自由
 *  函数，全部只用 PkString 的公开 API 实现（at / mid / right / left / ==），
 *  不触碰 PkString 的私有缓冲。Latin-1 算法逐位对齐 psdToLatin1（Qt toLatin1）。
 */

#pragma once

#include "PkString.h"

#include <string>
#include <vector>

// Latin-1 编码：码元 < 256 取低字节，否则写成 '?'（与 Qt QString::toLatin1 一致）。
inline std::string flakeToLatin1(const PkString &s)
{
    std::string out;
    out.reserve(static_cast<std::size_t>(s.size()));
    for (int i = 0; i < s.size(); ++i) {
        const char16_t c = s.at(i);
        out.push_back(static_cast<char>(c < 256 ? static_cast<unsigned char>(c) : '?'));
    }
    return out;
}

// 在 hay 中从 from（负则从末尾折算）起找 needle 的起始码元下标；找不到返回 -1。
// 空 needle 命中位置 0。对齐 Qt QString::indexOf 语义（UTF-16 码元下标）。
inline int flakeIndexOf(const PkString &hay, const PkString &needle, int from = 0)
{
    const int h = hay.size();
    const int n = needle.size();
    if (n == 0) {
        return 0;
    }
    if (n > h) {
        return -1;
    }
    int start = from;
    if (start < 0) {
        start += h;
    }
    if (start < 0) {
        start = 0;
    }
    const int last = h - n;
    for (int i = start; i <= last; ++i) {
        if (hay.mid(i, n) == needle) {
            return i;
        }
    }
    return -1;
}

// 后缀判定：对齐 Qt QString::endsWith。
inline bool flakeEndsWith(const PkString &s, const PkString &suffix)
{
    const int n = suffix.size();
    return n == 0 || (s.size() >= n && s.right(n) == suffix);
}

// 原地去掉末尾 n 个码元：对齐 Qt QString::chop。
inline void flakeChop(PkString &s, int n)
{
    const int sz = s.size();
    s = s.left(n >= sz ? 0 : sz - n);
}
