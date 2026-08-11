#pragma once

#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <type_traits>

// ---------------------------------------------------------------------------
// 浮点比较：逐条复刻 Qt 6 的语义。
//
// 为什么不能用 == ：QCOMPARE 对 float/double 走的是模糊比较，
// QCOMPARE(0.1 + 0.2, 0.3) 在 Qt 里通过。Krita 的测试大量依赖这条
// （例：TestKoIntegerMaths 的整数路径不受影响，但 libs/image 的几何测试全靠它）。
// 线级 spec 的对齐口径是"任何行为差异默认都是缺陷"，所以照抄规则，不自造 epsilon。
// ---------------------------------------------------------------------------

inline bool pkFuzzyCompare(double p1, double p2)
{
    return std::fabs(p1 - p2) * 1000000000000. <= std::fmin(std::fabs(p1), std::fabs(p2));
}

inline bool pkFuzzyCompare(float p1, float p2)
{
    return std::fabs(p1 - p2) * 100000.f <= std::fmin(std::fabs(p1), std::fabs(p2));
}

inline bool pkFuzzyIsNull(double d) { return std::fabs(d) <= 0.000000000001; }
inline bool pkFuzzyIsNull(float f)  { return std::fabs(f) <= 0.00001f; }

// floatingCompare 只按 **t1** 的分类分支，对 t1/t2 不对称。这是 Qt 的行为，
// PK_COMPARE(0.0, 1e-300) 与 PK_COMPARE(1e-300, 0.0) 结果不同。不要"修正"成对称。
template <typename T>
bool pkFloatingCompare(T t1, T t2)
{
    switch (std::fpclassify(t1)) {
    case FP_INFINITE:
        return (t1 < 0) == (t2 < 0) && std::fpclassify(t2) == FP_INFINITE;
    case FP_NAN:
        return std::fpclassify(t2) == FP_NAN;
    case FP_SUBNORMAL:
    case FP_ZERO:
        return pkFuzzyIsNull(t2);
    default:
        if (!pkFuzzyIsNull(t1)) {
            return pkFuzzyCompare(t1, t2);
        }
        return pkFuzzyIsNull(t2);
    }
}

// ---------------------------------------------------------------------------
// 比较分派：浮点走 pkFloatingCompare，其余走 operator==。
// ---------------------------------------------------------------------------

template <typename T1, typename T2>
typename std::enable_if<
    std::is_floating_point<typename std::decay<T1>::type>::value &&
    std::is_floating_point<typename std::decay<T2>::type>::value, bool>::type
pkTestCompare(const T1 &t1, const T2 &t2)
{
    using C = typename std::common_type<T1, T2>::type;
    return pkFloatingCompare<C>(static_cast<C>(t1), static_cast<C>(t2));
}

template <typename T1, typename T2>
typename std::enable_if<
    !(std::is_floating_point<typename std::decay<T1>::type>::value &&
      std::is_floating_point<typename std::decay<T2>::type>::value), bool>::type
pkTestCompare(const T1 &t1, const T2 &t2)
{
    return t1 == t2;
}

// ---------------------------------------------------------------------------
// 诊断字符串化。
//
// QTest 的 QTest::toString<T>() 默认返回 nullptr，打印 "<unprintable>"；
// 我们用 SFINAE 检测 ostream 可插入性，做到同一件事而不需要为每个类型写重载。
// **不得因为某个类型不可打印就编译失败** —— 那会让大量真实测试编不过。
// ---------------------------------------------------------------------------

template <typename T, typename = void>
struct PkTestStreamable : std::false_type {};

template <typename T>
struct PkTestStreamable<T, decltype(void(std::declval<std::ostream &>() << std::declval<const T &>()))>
    : std::true_type {};

template <typename T>
typename std::enable_if<PkTestStreamable<T>::value, std::string>::type
pkTestToString(const T &value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

template <typename T>
typename std::enable_if<!PkTestStreamable<T>::value, std::string>::type
pkTestToString(const T &)
{
    return "<unprintable>";
}

inline std::string pkTestToString(bool value) { return value ? "true" : "false"; }

// 失败信息的组装（非模板部分放 .cpp，避免每个 TU 重复实例化）
std::string pkTestCompareFailureMessage(const char *actualExpr, const char *expectedExpr,
                                        const std::string &actualStr, const std::string &expectedStr);
