#include "PkString.h"

#include <algorithm>

namespace {

// 全部按 UTF-16 码元下标操作：不做 Unicode 规范化、不做 locale 感知比较。
// 选型文档实测 localeAwareCompare / toCaseFolded / QCollator / NormalizationForm
// 在 Krita 核心里各 0 处用量，实现它们是 10 倍工作量买 0 收益。
bool pkIsSpace(char16_t c)
{
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r' || c == u'\f' || c == u'\v';
}

// 在 hay 里找 needle 的起始码元下标；找不到返回 -1。空 needle 命中位置 0。
int pkIndexOf(const std::vector<char16_t>& hay, const std::vector<char16_t>& needle)
{
    if (needle.empty()) {
        return 0;
    }
    if (needle.size() > hay.size()) {
        return -1;
    }
    std::vector<char16_t>::const_iterator it =
        std::search(hay.begin(), hay.end(), needle.begin(), needle.end());
    if (it == hay.end()) {
        return -1;
    }
    return static_cast<int>(it - hay.begin());
}

} // namespace

bool PkString::contains(const PkString& sub) const
{
    return pkIndexOf(_cbuf(), sub._cbuf()) >= 0;
}

PkString PkString::left(int n) const
{
    if (n <= 0) {
        return PkString();
    }
    if (n >= size()) {
        return *this;
    }
    return mid(0, n);
}

PkString PkString::right(int n) const
{
    if (n <= 0) {
        return PkString();
    }
    if (n >= size()) {
        return *this;
    }
    return mid(size() - n, n);
}

PkString PkString::mid(int pos, int n) const
{
    const int len = size();
    if (pos < 0 || pos >= len) {
        return PkString();
    }
    int take = (n < 0) ? (len - pos) : n;
    if (take > len - pos) {
        take = len - pos;
    }
    if (take <= 0) {
        return PkString();
    }
    const std::vector<char16_t>& b = _cbuf();
    PkString r;
    r._data().assign(b.begin() + pos, b.begin() + pos + take);
    return r;
}

bool PkString::startsWith(const PkString& prefix) const
{
    const std::vector<char16_t>& p = prefix._cbuf();
    const std::vector<char16_t>& b = _cbuf();
    if (p.size() > b.size()) {
        return false;
    }
    return std::equal(p.begin(), p.end(), b.begin());
}

PkString PkString::trimmed() const
{
    const std::vector<char16_t>& b = _cbuf();
    std::size_t begin = 0;
    std::size_t end = b.size();
    while (begin < end && pkIsSpace(b[begin])) {
        ++begin;
    }
    while (end > begin && pkIsSpace(b[end - 1])) {
        --end;
    }
    if (begin == 0 && end == b.size()) {
        return *this;
    }
    return mid(static_cast<int>(begin), static_cast<int>(end - begin));
}

// 保留空段，与 QString::split 的默认（KeepEmptyParts）行为一致：
// 空串 split 出一个空段，n 个分隔符切出 n+1 段。
std::vector<PkString> PkString::split(char16_t sep) const
{
    std::vector<PkString> out;
    const std::vector<char16_t>& b = _cbuf();
    std::size_t start = 0;
    for (std::size_t i = 0; i <= b.size(); ++i) {
        if (i == b.size() || b[i] == sep) {
            out.push_back(mid(static_cast<int>(start), static_cast<int>(i - start)));
            start = i + 1;
        }
    }
    return out;
}
