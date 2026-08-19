// pk/flags/PkFlags.h —— QFlags 的类型安全枚举位标志替代品
//
// 逐字对齐 Qt 5.15 <QtCore/qflags.h> 的运算符集与语义（本 plan「问 0」探针钉死的
// 每一条）。「抄的是行为，不是代码」——这里抄的是 QFlags 的可观测行为（位逻辑、
// 存储类型、运算符不对称、testFlag 的精确公式），结构上做了最小必要调整：
//
//   - 不引入 Qt 的 Q_DECL_* / QT_* 宏，用 C++17 的 constexpr/noexcept 直写
//   - 不实现 QIncompatibleFlag / Q_DECLARE_INCOMPATIBLE_FLAGS（保留范围 0 用量，判据①）
//   - 不实现 Q_NO_TYPESAFE_FLAGS 分支（那是 Qt 的编译开关，我们不需要）
//
// 运算符集的不对称是 Qt 的历史事实，必须照抄（负向编译探针已钉）：
//   & 有 int/uint/Enum 三重重载；| 与 ^ 只有 QFlags/Enum，没有 int。
//   抄错成「| 也加 int 重载」会让 PkFlags 比 Qt 更宽松——不影响已有调用点，
//   但违反「默认全对齐」，且 oracle 的负向探针 + unit test 会抓。
#pragma once

#include <initializer_list>
#include <type_traits>

class PkFlag
{
    int i;
public:
    constexpr inline PkFlag(int value) noexcept : i(value) {}
    constexpr inline operator int() const noexcept { return i; }
    constexpr inline PkFlag(unsigned int value) noexcept : i(int(value)) {}
    constexpr inline operator unsigned int() const noexcept { return static_cast<unsigned int>(i); }
};

template <typename Enum>
class PkFlags
{
    static_assert(sizeof(Enum) <= sizeof(int),
                  "PkFlags uses an int as storage, so an enum with underlying "
                  "long long will overflow.");
    static_assert(std::is_enum<Enum>::value, "PkFlags is only usable on enumeration types.");

public:
    typedef typename std::conditional<
            std::is_unsigned<typename std::underlying_type<Enum>::type>::value,
            unsigned int,
            signed int
        >::type Int;
    typedef Enum enum_type;

    constexpr inline PkFlags() noexcept : i(0) {}
    constexpr inline PkFlags(Enum flags) noexcept : i(Int(flags)) {}
    constexpr inline PkFlags(PkFlag flag) noexcept : i(flag) {}
    constexpr inline PkFlags(std::initializer_list<Enum> flags) noexcept
        : i(initializer_list_helper(flags.begin(), flags.end())) {}

    constexpr inline PkFlags &operator&=(int mask) noexcept { i &= mask; return *this; }
    constexpr inline PkFlags &operator&=(unsigned int mask) noexcept { i &= mask; return *this; }
    constexpr inline PkFlags &operator&=(Enum mask) noexcept { i &= Int(mask); return *this; }
    constexpr inline PkFlags &operator|=(PkFlags other) noexcept { i |= other.i; return *this; }
    constexpr inline PkFlags &operator|=(Enum other) noexcept { i |= Int(other); return *this; }
    constexpr inline PkFlags &operator^=(PkFlags other) noexcept { i ^= other.i; return *this; }
    constexpr inline PkFlags &operator^=(Enum other) noexcept { i ^= Int(other); return *this; }

    constexpr inline operator Int() const noexcept { return i; }

    constexpr inline PkFlags operator|(PkFlags other) const noexcept { return PkFlags(PkFlag(i | other.i)); }
    constexpr inline PkFlags operator|(Enum other) const noexcept { return PkFlags(PkFlag(i | Int(other))); }
    constexpr inline PkFlags operator^(PkFlags other) const noexcept { return PkFlags(PkFlag(i ^ other.i)); }
    constexpr inline PkFlags operator^(Enum other) const noexcept { return PkFlags(PkFlag(i ^ Int(other))); }
    constexpr inline PkFlags operator&(int mask) const noexcept { return PkFlags(PkFlag(i & mask)); }
    constexpr inline PkFlags operator&(unsigned int mask) const noexcept { return PkFlags(PkFlag(i & mask)); }
    constexpr inline PkFlags operator&(Enum other) const noexcept { return PkFlags(PkFlag(i & Int(other))); }
    constexpr inline PkFlags operator~() const noexcept { return PkFlags(PkFlag(~i)); }

    constexpr inline bool operator!() const noexcept { return !i; }

    constexpr inline bool testFlag(Enum flag) const noexcept
    { return (i & Int(flag)) == Int(flag) && (Int(flag) != 0 || i == Int(flag)); }
    constexpr inline PkFlags &setFlag(Enum flag, bool on = true) noexcept
    { return on ? (*this |= flag) : (*this &= ~Int(flag)); }

private:
    constexpr static inline Int initializer_list_helper(
        typename std::initializer_list<Enum>::const_iterator it,
        typename std::initializer_list<Enum>::const_iterator end) noexcept
    {
        return (it == end ? Int(0) : (Int(*it) | initializer_list_helper(it + 1, end)));
    }

    Int i;
};

// Q_DECLARE_FLAGS 的替代。名字用 PK_ 前缀（与 pk/test 的 PK_COMPARE 等一致）。
#define PK_DECLARE_FLAGS(Flags, Enum) typedef PkFlags<Enum> Flags;

// Q_DECLARE_OPERATORS_FOR_FLAGS 的替代。两个自由 operator|：enum|enum 与 enum|flags。
// 刻意**不含** Q_DECLARE_INCOMPATIBLE_FLAGS 那条（保留范围 0 用量，判据①）。
#define PK_DECLARE_OPERATORS_FOR_FLAGS(Flags) \
    constexpr inline PkFlags<Flags::enum_type> operator|(Flags::enum_type f1, Flags::enum_type f2) noexcept \
    { return PkFlags<Flags::enum_type>(f1) | f2; } \
    constexpr inline PkFlags<Flags::enum_type> operator|(Flags::enum_type f1, PkFlags<Flags::enum_type> f2) noexcept \
    { return f2 | f1; }
