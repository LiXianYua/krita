#include "PkString.h"

#include "PkStringData.h"

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
    std::size_t len;   // 含 '%' 的总长（2 或 3）
    int num;           // 1..99
};

// 扫出串里所有 `%n` / `%nn` 占位符。`%` 后不是数字的（`%p`、`%%`、末尾的 `%`）
// 一律跳过、原样保留 —— KoProgressProxy 的 "%1: %p%" 就靠这条。
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
        int num = 0;
        int digits = 0;
        while (j < b.size() && digits < 2 && b[j] >= u'0' && b[j] <= u'9') {
            num = num * 10 + static_cast<int>(b[j] - u'0');
            ++j;
            ++digits;
        }
        if (digits > 0 && num >= 1) {
            PkPlaceholder p;
            p.pos = i;
            p.len = j - i;
            p.num = num;
            out.push_back(p);
            i = j;
        } else {
            ++i;
        }
    }
    return out;
}

// 一次性替换：编号最小的占位符吃 args[0]，次小的吃 args[1]……
// **先定位再拼**，所以替换进去的内容里的 `%n` 不会被二次扫描。
std::vector<char16_t> pkSubstitute(const std::vector<char16_t>& src,
                                   const std::vector<const std::vector<char16_t>*>& args)
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
        const std::vector<char16_t>& rep = *args[it->second];
        out.insert(out.end(), rep.begin(), rep.end());
        cursor = ph[k].pos + ph[k].len;
    }
    out.insert(out.end(), src.begin() + static_cast<long>(cursor), src.end());
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
    r._detach();
    r._d->buf = pkSubstitute(_cbuf(), args);
    return r;
}

// 双参形式**同时**替换，不是 arg(a).arg(b)：a 里若含 %2 不会被 b 二次吃掉。
PkString PkString::arg(const PkString& a, const PkString& b) const
{
    std::vector<const std::vector<char16_t>*> args;
    args.push_back(&a._cbuf());
    args.push_back(&b._cbuf());
    PkString r;
    r._detach();
    r._d->buf = pkSubstitute(_cbuf(), args);
    return r;
}

PkString PkString::arg(int v) const
{
    char tmp[32];
    const std::to_chars_result r = std::to_chars(tmp, tmp + sizeof(tmp), v);
    return arg(PkString::PkFromUtf8(tmp, static_cast<int>(r.ptr - tmp)));
}

// 不能用 snprintf("%g")：printf 族的小数点字符由 LC_NUMERIC 决定，
// 在 de_DE 这类 locale 下会输出 "0,75"。QString::arg(double) 走的是
// QLocaleData::c()（固定 C locale），必须对齐。std::to_chars 由标准保证
// 与 locale 无关，general + 精度 6 等价于 C locale 下的 "%g"。
PkString PkString::arg(double v) const
{
    char tmp[64];
    const std::to_chars_result r =
        std::to_chars(tmp, tmp + sizeof(tmp), v, std::chars_format::general, 6);
    if (r.ec != std::errc()) {
        return *this;
    }
    return arg(PkString::PkFromUtf8(tmp, static_cast<int>(r.ptr - tmp)));
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

    bool good = false;
    double v = 0.0;
    if (!looksHex) {
        const locale_t cloc = pkCLocale();
        char* stop = nullptr;
        errno = 0;
        // newlocale 失败（基本不可能）时退回普通 strtod：宁可在那种环境下
        // 退化成受 locale 影响，也好过给 strtod_l 传一个空 locale_t（UB）。
        const double parsed = (cloc != static_cast<locale_t>(0))
                                  ? ::strtod_l(begin, &stop, cloc)
                                  : ::strtod(begin, &stop);

        // ERANGE 不能一刀切当失败：glibc 对**渐进下溢**（结果是次正规数、仍能
        // 精确表示，如 "1e-310" "4.9e-324"）也置 ERANGE，可那是成功的解析。
        // 真正该判失败的只有两头：上溢（返回 ±inf）与全下溢（返回 0）。
        // 合法的零（"0" "0.0"）根本不会置 ERANGE，所以不必单独排除。
        bool rangeOk = true;
        if (errno == ERANGE) {
            rangeOk = (parsed != 0.0) && std::isfinite(parsed);
        }

        if (stop != begin && rangeOk && pkTailIsBlank(stop, end)) {
            v = parsed;
            good = true;
        }
    }
    if (ok != nullptr) {
        *ok = good;
    }
    return good ? v : 0.0;
}
