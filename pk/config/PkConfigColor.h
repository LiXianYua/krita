#pragma once
#include <cstdint>

// PkConfigColor —— PkConfigGroup::readEntry/writeEntry(..., PkConfigColor) 的值类型。
// 只是一个 (r,g,b,a) 元组 + 相等比较，序列化格式（"r,g,b,a" 十进制逗号分隔）由
// PkConfigGroup.cpp 负责，本类本身不知道自己会被怎么持久化。
class PkConfigColor
{
public:
    PkConfigColor() : r(0), g(0), b(0), a(255) {}
    PkConfigColor(int r, int g, int b, int a = 255)
        : r(static_cast<uint8_t>(r)), g(static_cast<uint8_t>(g)),
          b(static_cast<uint8_t>(b)), a(static_cast<uint8_t>(a))
    {
    }

    uint8_t r, g, b, a;

    bool operator==(const PkConfigColor &other) const
    {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};
