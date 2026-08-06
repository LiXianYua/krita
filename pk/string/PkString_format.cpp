#include "PkString.h"

#include "PkStringData.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <map>
#include <system_error>

namespace {

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

// 跳过前导空白与可选的 '+'。from_chars 两者都不接受，而 QString 两者都容忍。
const char* pkSkipLeading(const char* p, const char* end)
{
    while (p != end && pkIsAsciiBlank(*p)) {
        ++p;
    }
    if (p != end && *p == '+') {
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
// 也**不用 strtol/strtod**：它们的小数点/数字分类由 LC_NUMERIC 决定，而
// Krita 运行时会 setlocale(LC_ALL, "")（Qt 的 initLocale），非英语系统下
// LC_NUMERIC 就不是 "C" 了 —— 那时 strtod("0.75") 会在小数点处停下、返回 0。
// QString::toDouble 硬编码 C locale（QLocaleData::c()），我们必须一样。
// std::from_chars 由标准保证与 locale 无关，是唯一不依赖全局状态的选择。
// 与 QString 一致地**拒绝尾随垃圾**，但容忍首尾空白与前导 '+'。
int PkString::toInt(bool* ok) const
{
    const std::string s = PkToUtf8();
    const char* const end = s.data() + s.size();
    const char* const begin = pkSkipLeading(s.data(), end);

    int v = 0;
    const std::from_chars_result r = std::from_chars(begin, end, v, 10);
    const bool good = (r.ec == std::errc()) && pkTailIsBlank(r.ptr, end);
    if (ok != nullptr) {
        *ok = good;
    }
    return good ? v : 0;
}

double PkString::toDouble(bool* ok) const
{
    const std::string s = PkToUtf8();
    const char* const end = s.data() + s.size();
    const char* const begin = pkSkipLeading(s.data(), end);

    double v = 0.0;
    const std::from_chars_result r =
        std::from_chars(begin, end, v, std::chars_format::general);
    const bool good = (r.ec == std::errc()) && pkTailIsBlank(r.ptr, end);
    if (ok != nullptr) {
        *ok = good;
    }
    return good ? v : 0.0;
}
