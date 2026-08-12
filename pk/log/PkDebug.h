#pragma once

// qDebug() << a << b 这种流式写法的零 Qt 替代品。
//
// 设计要点（细节见 .superpowers/sdd/R-08/task-2-brief.md「关键机制 §B」，
// 那份表是对真 Qt 探针跑出来的原始输出，不是从文档推出来的）：
//
// - PkDebug 按值复制、共享同一份底层状态（shared_ptr<Impl>）——这样一行
//   `qDebug() << a << b << c` 里链式产生的每个中间临时对象都在往同一块
//   缓冲区写，只有"这一行"最后一个副本析构时才真正 flush 一次。
// - operator<< 是**成员函数**而不是自由函数：brief 的示例里有一种真实存在
//   的用户重载形态——`PkDebug operator<<(PkDebug dbg, const Foo &f)` 按值收、
//   按值还。这意味着链式表达式里某一段的左操作数可能是一个纯右值（临时对象）。
//   非成员的 `operator<<(PkDebug &, T)`（左值引用形参）没法绑定到纯右值，
//   编不过；而成员函数可以在任意值类别的对象上调用，天然兼容这种链式写法
//   （这也是真实 Qt 里 QDebug 自己的 operator<< 家族按值收发的原因）。
// - `quote` 只影响"QString 族"，这里用鸭子类型 SFINAE 探测 `.PkToUtf8()`
//   成员（做法照抄 pk/test/PkTestCompare.h:82-103 的 decltype(void(...)) 手法）。
//   `const char*`/`char` 永不加引号。
// - `qSetFieldWidth` 的宽度存进共享状态、每次插入前重设——std::ostringstream
//   的 width() 是一次性的，不重设的话只有第一项被加宽。

#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "PkLogLevel.h"

// ---------------------------------------------------------------------------
// 鸭子类型探测：三条分支互斥（PkToUtf8 优先于 ostream，避免二义）。
// ---------------------------------------------------------------------------

template <typename T, typename = void>
struct PkDebugHasToUtf8 : std::false_type {};

template <typename T>
struct PkDebugHasToUtf8<T, decltype(void(std::declval<const T &>().PkToUtf8()))>
    : std::true_type {};

template <typename T, typename = void>
struct PkDebugIsOstreamable : std::false_type {};

template <typename T>
struct PkDebugIsOstreamable<
    T, decltype(void(std::declval<std::ostream &>() << std::declval<const T &>()))>
    : std::true_type {};

// ---------------------------------------------------------------------------
// qSetFieldWidth / qSetPadChar / qSetRealNumberPrecision 的对应物：
// 都是"造一个小 tag 结构体、operator<< 收到后改共享状态"的操纵符（manipulator）。
// ---------------------------------------------------------------------------

struct PkDebugFieldWidth { int width; };
inline PkDebugFieldWidth qSetFieldWidth(int width) { return PkDebugFieldWidth{width}; }

struct PkDebugPadChar { char ch; };
inline PkDebugPadChar qSetPadChar(char ch) { return PkDebugPadChar{ch}; }

struct PkDebugRealNumberPrecision { int precision; };
inline PkDebugRealNumberPrecision qSetRealNumberPrecision(int precision)
{
    return PkDebugRealNumberPrecision{precision};
}

class PkDebug
{
public:
    PkDebug(PkLogLevel level, const PkLogContext &ctx);
    PkDebug(const PkDebug &) = default;
    PkDebug &operator=(const PkDebug &) = default;
    ~PkDebug();

    PkDebug &space();
    PkDebug &nospace();
    PkDebug &quote();
    PkDebug &noquote();
    PkDebug &maybeSpace();

    // 内置类型各自的非模板重载——与下面的模板族在类型精确匹配时按标准的
    // "非模板优先于模板" 规则胜出，同时不吃泛型分支的鸭子类型探测。
    PkDebug &operator<<(const char *value);
    PkDebug &operator<<(char value);
    PkDebug &operator<<(bool value);

    PkDebug &operator<<(PkDebugFieldWidth w);
    PkDebug &operator<<(PkDebugPadChar p);
    PkDebug &operator<<(PkDebugRealNumberPrecision p);

    // 分支一：鸭子类型 PkToUtf8（PkString 族）——唯一受 quote 影响的一类。
    template <typename T>
    typename std::enable_if<PkDebugHasToUtf8<T>::value, PkDebug &>::type
    operator<<(const T &value)
    {
        applyFieldWidth();
        if (_s->quote) {
            _s->ts << '"' << value.PkToUtf8() << '"';
        } else {
            _s->ts << value.PkToUtf8();
        }
        return maybeSpace();
    }

    // 分支二：能塞进 std::ostream 的其它类型（数值、已有 operator<< 的类型……）。
    template <typename T>
    typename std::enable_if<!PkDebugHasToUtf8<T>::value && PkDebugIsOstreamable<T>::value,
                             PkDebug &>::type
    operator<<(const T &value)
    {
        applyFieldWidth();
        _s->ts << value;
        return maybeSpace();
    }

    // 分支三：两条都不占——不可打印类型不能让编译失败，退化成占位文本。
    template <typename T>
    typename std::enable_if<!PkDebugHasToUtf8<T>::value && !PkDebugIsOstreamable<T>::value,
                             PkDebug &>::type
    operator<<(const T &)
    {
        applyFieldWidth();
        _s->ts << "<unprintable>";
        return maybeSpace();
    }

private:
    friend PkDebug PkDebugMakeSilent();

    struct Impl
    {
        std::ostringstream ts;
        bool space = true;
        bool quote = true;
        PkLogLevel level = PkLogDebug;
        PkLogContext ctx;
        bool silent = false;
        // sticky field width：ostringstream::width() 每次格式化输出后自动清零，
        // 所以要在每次插入前（值本身、以及 maybeSpace/space 写的分隔符）重设。
        int fieldWidth = 0;
    };

    void applyFieldWidth();
    void writeSeparator();

    std::shared_ptr<Impl> _s;
};

// ---------------------------------------------------------------------------
// 仅供测试用的脚手架（评审 Minor 项：公开头里没有任何"仅测试用"标注，容易
// 被当成正式 API 误用）。业务调用点应该走 PkMessageLogger / qC*/qDebug 家族
// 拿 PkDebug，不要直接调这两个函数。
// ---------------------------------------------------------------------------

// 测试专用构造：直接给级别与分类名，跳过 file/line/function（Task 3/4 的
// PkLoggingCategory / qDebug() 自由函数才会填那些）。
PkDebug PkDebugMakeForTest(PkLogLevel level, const char *category);

// 禁用路径用：flush 时什么都不产出（供后续 qDebug()-禁用分类走这条路）。
PkDebug PkDebugMakeSilent();
