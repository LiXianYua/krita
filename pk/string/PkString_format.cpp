#include "PkString.h"
#include "PkStringCodec.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <map>
#include <system_error>

// POSIX 的 per-locale 解析接口。用 C 头而不是 <clocale>/<cstdlib>：
// newlocale/locale_t 只在 <locale.h> 里，strtod_l 只在 <stdlib.h> 里，
// 对应的 C++ 包装头不保证暴露这些 POSIX 扩展。
#include <cerrno>
#include <locale.h>
#include <stdlib.h>

namespace {

// ── 为什么解析走 strtod_l 而不是 std::from_chars ──────────────────
// 目标平台是 Android NDK（bionic + libc++）。libc++ 的 from_chars **浮点**重载
// 直到 LLVM 20 才实现，而 NDK r27/r28 只带 Clang 18/19 —— 在那上面
// std::from_chars(..., double&, ...) 是编译期错误（no matching function），
// 不是运行时问题。宿主机 g++/libstdc++ 从 GCC 11 起就有浮点 from_chars，
// 所以整条判据链路（replacement.sh 全程用宿主工具链）看不见这个差异。
//
// strtod_l + newlocale("C") 是 glibc / bionic / MSVC(_strtod_l) 都提供的老接口，
// 与 libc++ 版本无关。它同时满足「不受 LC_NUMERIC 影响」这条硬要求：
// 传进去的是显式的 C locale，不读进程全局 locale。
//
// **只有解析方向换回来。** to_chars（写方向，arg(double)/arg(int) 用）
// libc++ v14 起就有，NDK 上没问题，不动。整数 from_chars 同理（libc++ v7 起）。
locale_t pkCLocale()
{
    // 进程生命周期缓存一份；不 freelocale（故意的，就一个对象）。
    // 函数内 static 的初始化由 C++11 保证线程安全。
    static locale_t loc = ::newlocale(LC_ALL_MASK, "C", static_cast<locale_t>(0));
    return loc;
}

struct PkPlaceholder {
    std::size_t pos;   // '%' 的下标
    std::size_t len;   // 含 '%'（与可能的 'L'）的总长
    int num;           // 0..99
    bool locale;        // 是否是 %L<N> 形式
};

// 扫出串里所有 `%n` / `%Ln` 占位符（n 是 0..99 的 1 或 2 位数字）。`%` 后既不是
// 可选的 'L' 也不是数字的（`%p`、`%%`、末尾的 `%`）一律跳过、原样保留——
// KoProgressProxy 的 "%1: %p%" 就靠这条。
// **num 允许 0**：真实 Qt 的占位符编号从 0 开始，不是从 1 开始
// （背景 ④："%0-%1".arg("x") 替换的是 %0，不是 %1）。
std::vector<PkPlaceholder> pkScanPlaceholders(const std::vector<char16_t>& b)
{
    std::vector<PkPlaceholder> out;
    std::size_t i = 0;
    while (i < b.size()) {
        if (b[i] != u'%') {
            ++i;
            continue;
        }
        std::size_t j = i + 1;
        bool locale = false;
        if (j < b.size() && b[j] == u'L') {
            locale = true;
            ++j;
        }
        int num = 0;
        int digits = 0;
        while (j < b.size() && digits < 2 && b[j] >= u'0' && b[j] <= u'9') {
            num = num * 10 + static_cast<int>(b[j] - u'0');
            ++j;
            ++digits;
        }
        if (digits > 0) {
            PkPlaceholder p;
            p.pos = i;
            p.len = j - i;
            p.num = num;
            p.locale = locale;
            out.push_back(p);
            i = j;
        } else {
            ++i;   // 'L' 后面没跟数字（比如裸 "%L" 或 "%Lx"）：不是占位符，原样保留
        }
    }
    return out;
}

// 一次性替换：编号最小的占位符吃 args[0]，次小的吃 args[1]……
// **先定位再拼**，所以替换进去的内容里的 `%n` 不会被二次扫描。
// isNumericArg[k] 标记 args[k] 是否来自 arg(int)（只有数字实参才可能被 %L 分组）。
// groupedArgs[k]（仅当 isNumericArg[k] 为真时有效）是 args[k] 千分位分组之后的
// 文本——每个占位符出现位置各自决定用 args[k] 还是 groupedArgs[k]：
//   普通 %N       -> 用 args[k]
//   %LN 且数字实参 -> 用 groupedArgs[k]
//   %LN 但非数字实参（字符串/浮点） -> 仍用 args[k]（L 标志没有效果）
std::vector<char16_t> pkSubstitute(const std::vector<char16_t>& src,
                                   const std::vector<const std::vector<char16_t>*>& args,
                                   const std::vector<bool>& isNumericArg,
                                   const std::vector<std::vector<char16_t>>& groupedArgs)
{
    const std::vector<PkPlaceholder> ph = pkScanPlaceholders(src);
    if (ph.empty() || args.empty()) {
        return src;
    }

    std::vector<int> nums;
    nums.reserve(ph.size());
    for (std::size_t k = 0; k < ph.size(); ++k) {
        nums.push_back(ph[k].num);
    }
    std::sort(nums.begin(), nums.end());
    nums.erase(std::unique(nums.begin(), nums.end()), nums.end());

    std::map<int, std::size_t> numToArg;
    const std::size_t n = std::min(nums.size(), args.size());
    for (std::size_t k = 0; k < n; ++k) {
        numToArg[nums[k]] = k;
    }

    std::vector<char16_t> out;
    out.reserve(src.size());
    std::size_t cursor = 0;
    for (std::size_t k = 0; k < ph.size(); ++k) {
        std::map<int, std::size_t>::const_iterator it = numToArg.find(ph[k].num);
        if (it == numToArg.end()) {
            continue;   // 本轮没轮到这个编号，原样留着给下一次 arg
        }
        out.insert(out.end(), src.begin() + static_cast<long>(cursor),
                              src.begin() + static_cast<long>(ph[k].pos));
        const std::size_t argIdx = it->second;
        const bool useGrouped = ph[k].locale
                              && argIdx < isNumericArg.size()
                              && argIdx < groupedArgs.size()
                              && isNumericArg[argIdx];
        const std::vector<char16_t>& rep = useGrouped ? groupedArgs[argIdx] : *args[argIdx];
        out.insert(out.end(), rep.begin(), rep.end());
        cursor = ph[k].pos + ph[k].len;
    }
    out.insert(out.end(), src.begin() + static_cast<long>(cursor), src.end());
    return out;
}

// 千分位分组：从个位往前每 3 位插一个逗号，符号（若有）不参与分组。
// 只用于 %L<N> 占位符遇到数字实参的情形（arg(int) 的实参）——固定逗号分组，
// 不跟随运行时 locale（理由：R-13 plan 背景 ⑤，0 真实调用点，且与
// arg(double)/toDouble 已有的"locale 无关"设计保持一致）。
std::vector<char16_t> pkGroupDigits(const std::vector<char16_t>& digits)
{
    std::size_t start = 0;
    bool negative = false;
    if (!digits.empty() && digits[0] == u'-') {
        negative = true;
        start = 1;
    }
    const std::size_t n = digits.size() - start;
    if (n <= 3) {
        return digits;   // 不足 4 位不分组（背景 ⑤："999" -> "999"）
    }
    std::vector<char16_t> out;
    if (negative) {
        out.push_back(u'-');
    }
    const std::size_t firstGroupLen = n % 3 == 0 ? 3 : n % 3;
    out.insert(out.end(), digits.begin() + static_cast<long>(start),
                          digits.begin() + static_cast<long>(start + firstGroupLen));
    for (std::size_t i = start + firstGroupLen; i < digits.size(); i += 3) {
        out.push_back(u',');
        out.insert(out.end(), digits.begin() + static_cast<long>(i),
                              digits.begin() + static_cast<long>(i + 3));
    }
    return out;
}

// arg(double) 的 %L 分组：只分组小数点之前的整数部分，小数点及其后内容原样
// 拼回去；符号不参与分组。真实 Qt 5.15.7 实测（R-13 最终评审 C1(b) 探针）：
//   "[%L1]".arg(1234.5)   -> "[1,234.5]"    （只分组整数部分）
//   "[%L1]".arg(999.5)    -> "[999.5]"      （整数部分不足 1000 不分组）
//   "[%L1]".arg(-1234.5)  -> "[-1,234.5]"   （符号不参与分组）
// 复用 pkGroupDigits（它假设整串是"可选符号+纯数字"，没有小数点）：先切出
// 小数点之前的子串交给它分组，再把小数点及之后原样拼回来。
std::vector<char16_t> pkGroupDecimal(const std::vector<char16_t>& digits)
{
    std::size_t dot = digits.size();
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (digits[i] == u'.') {
            dot = i;
            break;
        }
    }
    const std::vector<char16_t> intPart(digits.begin(), digits.begin() + static_cast<long>(dot));
    std::vector<char16_t> out = pkGroupDigits(intPart);
    out.insert(out.end(), digits.begin() + static_cast<long>(dot), digits.end());
    return out;
}

// arg(int, fieldWidth) 与 arg(int) 共用的补宽度算法：fieldWidth<0 左对齐、
// 否则右对齐，宽度取绝对值，长度已经 >= width 就不补。操作对象是 char16_t
// 缓冲区而不是 std::string——调用方需要分别对"原始数字串"与"分组后数字串"
// 各跑一遍（真实 Qt 5.15.7 实测：%L<N> 占位符按分组后含逗号的长度补宽度，
// 普通 %N 占位符按原始数字长度补宽度，见 arg(int,fieldWidth) 上方注释）。
std::vector<char16_t> pkPadField(const std::vector<char16_t>& digits, int fieldWidth)
{
    // fieldWidth == INT_MIN 时 `-fieldWidth` 是有符号整数溢出（UB）：实测会转成
    // 一个天文数字的 size_t，让下面的 vector 补齐操作抛 std::length_error 崩溃，
    // 而真实 Qt 在这个极端输入下正常返回（R-13 最终评审 I5）。改成无符号回绕：
    // `0u - (size_t)fieldWidth` 对 fieldWidth==INT_MIN 是良定义的，算出来恰好是
    // |INT_MIN|（模 2^64 意义下）；fieldWidth 为其余负值时同样成立。
    const std::size_t width = (fieldWidth < 0)
        ? (static_cast<std::size_t>(0) - static_cast<std::size_t>(fieldWidth))
        : static_cast<std::size_t>(fieldWidth);
    if (digits.size() >= width) {
        return digits;
    }
    const std::vector<char16_t> pad(width - digits.size(), u' ');
    std::vector<char16_t> out;
    out.reserve(width);
    if (fieldWidth < 0) {
        out.insert(out.end(), digits.begin(), digits.end());
        out.insert(out.end(), pad.begin(), pad.end());
    } else {
        out.insert(out.end(), pad.begin(), pad.end());
        out.insert(out.end(), digits.begin(), digits.end());
    }
    return out;
}

// 刻意不使用 std::isspace：它受 LC_CTYPE 影响。数值解析的每一个环节都必须
// 与进程 locale 无关（理由见下面 toDouble 的注释）。
bool pkIsAsciiBlank(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool pkTailIsBlank(const char* p, const char* end)
{
    while (p != end) {
        if (!pkIsAsciiBlank(*p)) {
            return false;
        }
        ++p;
    }
    return true;
}

// 大小写不敏感地比较 [p, p+n) 与字面量 lit（lit 已经是全小写）。
bool pkCiEquals(const char* p, std::size_t n, const char* lit)
{
    for (std::size_t i = 0; i < n; ++i) {
        char c = p[i];
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
        if (c != lit[i]) {
            return false;
        }
    }
    return true;
}

// 整数路径的起点校验。文法与 strtod 一致：空白 → **一个**符号 → 数字。
// 返回该交给 from_chars 的起点；文法不合返回 nullptr。
//
// 为什么不能只是「跳空白 + 剥掉一个 '+'」：整数 from_chars 不认 '+'，
// **但认 '-'**。只剥 '+' 的话 "+-3" 剩下 "-3"，被 from_chars 正常解析成 -3,
// 双重符号就这么溜过去了（QString 是 0/false）。
// 这跟 toDouble 那边「预剥符号让 strtod_l 重开一轮前缀扫描」是同一个坑的
// 两个分支：**在一个自己也做前缀扫描的解析器外面再做一遍前缀处理 = 放宽文法**。
const char* pkIntStart(const char* p, const char* end)
{
    while (p != end && pkIsAsciiBlank(*p)) {
        ++p;
    }
    const char* afterSign = p;
    bool negative = false;
    if (afterSign != end && (*afterSign == '+' || *afterSign == '-')) {
        negative = (*afterSign == '-');
        ++afterSign;
    }
    if (afterSign == end || *afterSign < '0' || *afterSign > '9') {
        return nullptr;   // 符号后面不是数字 —— 文法不合，直接判失败
    }
    // from_chars 认 '-' 不认 '+'：负数连符号一起交给它（这样 INT_MIN 才正确，
    // 自己取负会溢出）；正数与无符号从第一个数字交起。
    return negative ? p : afterSign;
}

// strtod_l 真正开始消费数字的位置：先跳空白，再认**一个**符号，然后就该是数字了
// （C 的 strtod 文法不允许符号后面再有空白）。十六进制闸门必须建在这个位置上，
// 只查「我们自己跳过之后的那一个位置」是不够的 —— 那样 "+ 0x10" 会从底下溜过去。
const char* pkNumberStart(const char* p, const char* end)
{
    while (p != end && pkIsAsciiBlank(*p)) {
        ++p;
    }
    if (p != end && (*p == '+' || *p == '-')) {
        ++p;
    }
    return p;
}

} // namespace

PkString& PkString::append(const PkString& other)
{
    return *this += other;
}

PkString PkString::arg(const PkString& a) const
{
    std::vector<const std::vector<char16_t>*> args;
    args.push_back(&a._cbuf());
    PkString r;
    r._data() = pkSubstitute(_cbuf(), args, {false}, {});
    return r;
}

// 双参形式**同时**替换，不是 arg(a).arg(b)：a 里若含 %2 不会被 b 二次吃掉。
PkString PkString::arg(const PkString& a, const PkString& b) const
{
    std::vector<const std::vector<char16_t>*> args;
    args.push_back(&a._cbuf());
    args.push_back(&b._cbuf());
    PkString r;
    r._data() = pkSubstitute(_cbuf(), args, {false, false}, {});
    return r;
}

// 三参形式**同时**替换，与双参版本同一套机制——不是 arg(a).arg(b).arg(c)。
// 真实调用点：libs/global/kis_assert.cpp:120、
// libs/flake/text/KoFFWWSConverter.cpp:193。
PkString PkString::arg(const PkString& a, const PkString& b, const PkString& c) const
{
    std::vector<const std::vector<char16_t>*> args;
    args.push_back(&a._cbuf());
    args.push_back(&b._cbuf());
    args.push_back(&c._cbuf());
    PkString r;
    r._data() = pkSubstitute(_cbuf(), args, {false, false, false}, {});
    return r;
}

PkString PkString::arg(int v) const
{
    char tmp[32];
    const std::to_chars_result res = std::to_chars(tmp, tmp + sizeof(tmp), v);
    const std::vector<char16_t> digits = PkStringCodec::FromUtf8(tmp, static_cast<std::size_t>(res.ptr - tmp));
    const std::vector<char16_t> grouped = pkGroupDigits(digits);

    std::vector<const std::vector<char16_t>*> args;
    args.push_back(&digits);
    PkString r;
    r._data() = pkSubstitute(_cbuf(), args, {true}, {grouped});
    return r;
}

// QString::arg(int a, int fieldWidth, int base=10, QChar fillChar=' ') 的
// base==10、fillChar==' ' 这一支——真实调用点 libs/global/KisRectsGrid.cpp:23
// 唯一用到的形态。fieldWidth 为正右对齐、为负左对齐，符号计入宽度。
//
// %L<N> 占位符先分组再按分组后长度补宽度，普通 %N 占位符按原始数字长度补
// 宽度，两者的判据与 arg(int)（单参版本）共用同一套 isNumericArg/groupedArgs
// 机制（pkSubstitute 按占位符自己带不带 L 标志去两者之间选）。真实 Qt 5.15.7
// 实测（旧注释在此处的断言与之相悖，已订正）：
//   "[%L1]".arg(1234567, 12)  -> "[   1,234,567]"（分组后 9 字符，补 3 空格到 12）
//   "[%L1]".arg(1234567, -12) -> "[1,234,567   ]"（负宽度左对齐，补在分组结果右边）
//   "[%L1]".arg(-1234567, 14) -> "[    -1,234,567]"（符号不参与分组，但计入宽度）
PkString PkString::arg(int v, int fieldWidth) const
{
    char tmp[32];
    const std::to_chars_result res = std::to_chars(tmp, tmp + sizeof(tmp), v);
    const std::vector<char16_t> digits = PkStringCodec::FromUtf8(tmp, static_cast<std::size_t>(res.ptr - tmp));
    const std::vector<char16_t> grouped = pkGroupDigits(digits);

    const std::vector<char16_t> paddedDigits = pkPadField(digits, fieldWidth);
    const std::vector<char16_t> paddedGrouped = pkPadField(grouped, fieldWidth);

    std::vector<const std::vector<char16_t>*> args;
    args.push_back(&paddedDigits);
    PkString r;
    r._data() = pkSubstitute(_cbuf(), args, {true}, {paddedGrouped});
    return r;
}

// 不能用 snprintf("%g")：printf 族的小数点字符由 LC_NUMERIC 决定，
// 在 de_DE 这类 locale 下会输出 "0,75"。QString::arg(double) 走的是
// QLocaleData::c()（固定 C locale），必须对齐。std::to_chars 由标准保证
// 与 locale 无关，general + 精度 6 等价于 C locale 下的 "%g"。
//
// 支持 %L 分组（R-13 最终评审 C1(b)）：与 arg(int) 共用 pkSubstitute 的
// isNumericArg/groupedArgs 机制——digits 是 to_chars 的原始结果，grouped 是
// pkGroupDecimal 分组后的版本；结果字符串里出现 'e'（科学计数法形态）就不分组
// （grouped=digits），真实 Qt 在这种形态下也不分组（探针：
// "[%L1]".arg(1000000.0) -> "[1e+06]"，不是 "[1,000,000]" 或对指数部分分组）。
PkString PkString::arg(double v) const
{
    std::vector<char16_t> digits;
    // -0.0 与 +0.0 都要落到字面 "0"：std::to_chars 会给 -0.0 输出 "-0"，
    // 但真实 Qt QString("%1").arg(-0.0) 输出 "0"（丢符号，R-13 最终评审 C1(a)）。
    // `v == 0.0` 这个比较对 +0.0/-0.0 都成立，正零走这条分支也没问题——结果
    // 一样，只是走了近路，不用等 to_chars 再去判断输出是不是 "-0"。
    if (v == 0.0) {
        digits = PkStringCodec::FromUtf8("0", 1);
    } else {
        char tmp[64];
        const std::to_chars_result r =
            std::to_chars(tmp, tmp + sizeof(tmp), v, std::chars_format::general, 6);
        if (r.ec != std::errc()) {
            return *this;
        }
        digits = PkStringCodec::FromUtf8(tmp, static_cast<std::size_t>(r.ptr - tmp));
    }

    const bool isSci = std::find(digits.begin(), digits.end(), u'e') != digits.end()
                     || std::find(digits.begin(), digits.end(), u'E') != digits.end();
    const std::vector<char16_t> grouped = isSci ? digits : pkGroupDecimal(digits);

    std::vector<const std::vector<char16_t>*> args;
    args.push_back(&digits);
    PkString r;
    r._data() = pkSubstitute(_cbuf(), args, {true}, {grouped});
    return r;
}

// ok 出参语义（不是异常）：所以不用 std::stoi。
// 也**不用裸的 strtol**：它的数字分类受 LC_NUMERIC 影响，而 Krita 运行时会
// setlocale(LC_ALL, "")（Qt 的 initLocale），非英语系统下 LC_NUMERIC 不是 "C"；
// QString::toInt 则硬编码 C locale（QLocaleData::c()），我们必须一样。
// 这里用**整数** from_chars：它由标准保证与 locale 无关，且整数重载 libc++ v7
// 起就有，NDK 上没问题 —— 只有浮点重载要等 libc++ v20，那条见 toDouble 上面的
// 注释与 pkCLocale()。
// 与 QString 一致地**拒绝尾随垃圾**，但容忍首尾空白与前导 '+'（只许一个符号，
// 且符号后必须紧跟数字 —— 见 pkIntStart）。
int PkString::toInt(bool* ok) const
{
    const std::string s = PkToUtf8();
    const char* const end = s.data() + s.size();
    const char* const begin = pkIntStart(s.data(), end);

    int v = 0;
    bool good = false;
    if (begin != nullptr) {
        const std::from_chars_result r = std::from_chars(begin, end, v, 10);
        good = (r.ec == std::errc()) && pkTailIsBlank(r.ptr, end);
    }
    if (ok != nullptr) {
        *ok = good;
    }
    return good ? v : 0;
}

double PkString::toDouble(bool* ok) const
{
    // c_str() 而不是 data()：strtod_l 要一个 NUL 结尾的串。但**尾随垃圾的判定
    // 仍用 s.size() 算出的 end** —— 串里若嵌了 U+0000，strtod 会在那儿停下，
    // 而 end 在更后面，NUL 本身不是空白，于是「NUL 之后还有字符」会被正确拒收。
    const std::string s = PkToUtf8();
    const char* const begin = s.c_str();
    const char* const end = begin + s.size();

    // **原样把整个串交给 strtod_l**，不预先剥掉空白或 '+'。预剥会让 strtod
    // 重新开一轮「跳空白 + 认符号」，于是 "+ 0x10" "++3.5" "+  -2.5" "+\n7"
    // 这些 C 文法本不接受的组合全被放行（QString 也不接受）。
    //
    // strtod 会把 "0x10" / "0x1p3" 当十六进制浮点吃掉，QString::toDouble 不接受
    // 十六进制。闸门建在 strtod 真正开始消费数字的位置上。
    const char* const numStart = pkNumberStart(begin, end);
    const bool looksHex = (end - numStart) >= 2 && numStart[0] == '0'
                          && (numStart[1] == 'x' || numStart[1] == 'X');

    // 背景 ⑦ 之一：strtod_l 会把完整单词 "infinity"（8 字母，大小写不敏感）当
    // 合法 token 整个吃掉，但真实 QString::toDouble 只认 "inf"（3 字母），拒收
    // "infinity"。显式判断：从 numStart 起紧跟 8 个字母恰好是 infinity 且立即
    // 到达空白/串尾 —— 提前判失败，不让它走到 strtod_l。
    const bool looksInfinity = (end - numStart) >= 8 && pkCiEquals(numStart, 8, "infinity")
                              && pkTailIsBlank(numStart + 8, end);

    // 背景 ⑦ 之二：strtod_l 允许 nan 前面带一个可选符号（C99 文法），但真实
    // QString::toDouble 只认裸 "nan"，带符号的 "+nan"/"-nan" 一律拒收。
    // numStart 已经跳过了那个可选符号（pkNumberStart 的行为），所以这里只要看
    // begin 到 numStart 之间是否真的出现过符号字符即可判断"带没带符号"。
    bool sawSign = false;
    {
        const char* p = begin;
        while (p != numStart && pkIsAsciiBlank(*p)) {
            ++p;
        }
        sawSign = (p != numStart);   // p 停在 numStart 之前说明中间是符号字符
    }
    const bool looksSignedNan = sawSign && (end - numStart) >= 3
                              && pkCiEquals(numStart, 3, "nan")
                              && pkTailIsBlank(numStart + 3, end);

    bool good = false;
    double v = 0.0;
    if (!looksHex && !looksInfinity && !looksSignedNan) {
        const locale_t cloc = pkCLocale();
        char* stop = nullptr;
        errno = 0;
        // newlocale 失败（基本不可能）时退回普通 strtod：宁可在那种环境下
        // 退化成受 locale 影响，也好过给 strtod_l 传一个空 locale_t（UB）。
        const double parsed = (cloc != static_cast<locale_t>(0))
                                  ? ::strtod_l(begin, &stop, cloc)
                                  : ::strtod(begin, &stop);

        // 背景 ⑦a：ERANGE 分两种失败，返回值不同。
        //   上溢（parsed 是 ±inf，非零）  -> 失败，但 v 必须是那个真实的 ±inf
        //   全下溢（parsed 恰好是 0）     -> 失败，v 就是 0（本来就该是 0，不是
        //                                  "该返点什么却被清零"）
        //   渐进下溢（parsed 是非零有限值，即次正规数）-> 不算失败，见下面 rangeOk
        bool rangeOk = true;
        bool overflowed = false;
        if (errno == ERANGE) {
            if (parsed != 0.0 && !std::isfinite(parsed)) {
                overflowed = true;
                rangeOk = false;
            } else if (parsed == 0.0) {
                rangeOk = false;
            }
            // else：非零有限值（次正规数）——rangeOk 保持 true，走成功路径
        }

        if (stop != begin && pkTailIsBlank(stop, end)) {
            if (rangeOk) {
                v = parsed;
                good = true;
            } else if (overflowed) {
                // ok=false，但真实 Qt 在这里返回算出来的 ±inf，不是 0.0
                // （背景 ⑦a 的探针：toDouble("1e400")/toDouble("1e309") 都是
                // ok=0 v=inf，不是 ok=0 v=0）。
                v = parsed;
            }
            // 其余情形（total underflow，rangeOk=false 且 overflowed=false）：
            // v 保持函数开头初始化的 0.0，good 保持 false。
        }
    }
    if (ok != nullptr) {
        *ok = good;
    }
    return v;   // 不再是 "good ? v : 0.0"——v 已经按上面的分支被正确设置过
}
