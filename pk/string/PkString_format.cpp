#include "PkString.h"

#include "PkStringData.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <map>

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

bool pkTailIsBlank(const char* p)
{
    while (*p != '\0') {
        if (std::isspace(static_cast<unsigned char>(*p)) == 0) {
            return false;
        }
        ++p;
    }
    return true;
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
    std::snprintf(tmp, sizeof(tmp), "%d", v);
    return arg(PkString(tmp));
}

PkString PkString::arg(double v) const
{
    char tmp[64];
    std::snprintf(tmp, sizeof(tmp), "%g", v);
    return arg(PkString(tmp));
}

// ok 出参语义（不是异常）：所以用 strtol/strtod 而不是 std::stoi。
// 与 QString::toInt 一致地**拒绝尾随垃圾**，但容忍首尾空白。
int PkString::toInt(bool* ok) const
{
    const std::string s = PkToUtf8();
    const char* begin = s.c_str();
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(begin, &end, 10);
    bool good = (end != begin) && pkTailIsBlank(end);
    if (good && (errno == ERANGE || v < static_cast<long>(INT_MIN) || v > static_cast<long>(INT_MAX))) {
        good = false;
    }
    if (ok != nullptr) {
        *ok = good;
    }
    return good ? static_cast<int>(v) : 0;
}

double PkString::toDouble(bool* ok) const
{
    const std::string s = PkToUtf8();
    const char* begin = s.c_str();
    char* end = nullptr;
    errno = 0;
    const double v = std::strtod(begin, &end);
    bool good = (end != begin) && pkTailIsBlank(end) && errno != ERANGE;
    if (ok != nullptr) {
        *ok = good;
    }
    return good ? v : 0.0;
}
