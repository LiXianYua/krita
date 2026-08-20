#include "PkString.h"

#include "unicode_case/PkUnicodeCaseData.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace {

// 全部按 UTF-16 码元下标操作：不做 Unicode 规范化、不做 locale 感知比较。
// 选型文档实测 localeAwareCompare / toCaseFolded / QCollator / NormalizationForm
// 在 Krita 核心里各 0 处用量，实现它们是 10 倍工作量买 0 收益。
// 对齐真实 Qt QChar::isSpace() 的完整 Unicode White_Space 集合，不只是
// ASCII+NBSP（R-13 背景 ③）——trimmed() 靠这份表判断该从两端剥掉哪些码元。
bool pkIsSpace(char16_t c)
{
    switch (c) {
    case 0x0009: case 0x000A: case 0x000B: case 0x000C: case 0x000D:
    case 0x0020: case 0x0085: case 0x00A0: case 0x1680:
    case 0x2000: case 0x2001: case 0x2002: case 0x2003: case 0x2004:
    case 0x2005: case 0x2006: case 0x2007: case 0x2008: case 0x2009:
    case 0x200A: case 0x2028: case 0x2029: case 0x202F: case 0x205F:
    case 0x3000:
        return true;
    default:
        return false;
    }
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

std::uint32_t pkNextCodePoint(const std::vector<char16_t>& input, std::size_t& index)
{
    const std::uint32_t first = input[index++];
    if (first >= 0xD800 && first <= 0xDBFF && index < input.size()) {
        const std::uint32_t second = input[index];
        if (second >= 0xDC00 && second <= 0xDFFF) {
            ++index;
            return 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
        }
    }
    return first;
}

template<std::size_t N>
const PkUnicodeCaseData::Mapping* pkFindCaseMapping(
    std::uint32_t codePoint,
    const PkUnicodeCaseData::Mapping (&mappings)[N])
{
    const PkUnicodeCaseData::Mapping* const begin = mappings;
    const PkUnicodeCaseData::Mapping* const end = mappings + N;
    const PkUnicodeCaseData::Mapping* const it =
        std::lower_bound(begin, end, codePoint,
                         [](const PkUnicodeCaseData::Mapping& mapping, std::uint32_t value) {
                             return mapping.source < value;
                         });
    return it != end && it->source == codePoint ? it : nullptr;
}

void pkAppendCodePoint(std::vector<char16_t>& output, std::uint32_t codePoint)
{
    if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char16_t>(codePoint));
        return;
    }
    const std::uint32_t value = codePoint - 0x10000;
    output.push_back(static_cast<char16_t>(0xD800 + (value >> 10)));
    output.push_back(static_cast<char16_t>(0xDC00 + (value & 0x3FF)));
}

template<std::size_t N, std::size_t M>
bool pkConvertCase(const std::vector<char16_t>& input,
                   std::vector<char16_t>& output,
                   const PkUnicodeCaseData::Mapping (&mappings)[N],
                   const char16_t (&values)[M])
{
    output.reserve(input.size());
    bool changed = false;
    for (std::size_t index = 0; index < input.size();) {
        const std::uint32_t codePoint = pkNextCodePoint(input, index);
        const PkUnicodeCaseData::Mapping* const mapping =
            pkFindCaseMapping(codePoint, mappings);
        if (mapping == nullptr) {
            pkAppendCodePoint(output, codePoint);
            continue;
        }
        changed = true;
        output.insert(output.end(), values + mapping->offset,
                      values + mapping->offset + mapping->length);
    }
    return changed;
}

} // namespace

bool PkString::contains(const PkString& sub) const
{
    return pkIndexOf(_cbuf(), sub._cbuf()) >= 0;
}

PkString PkString::left(int n) const
{
    const std::size_t sz = _cbuf().size();
    if (static_cast<unsigned>(n) >= static_cast<unsigned>(sz)) {
        return *this;
    }
    return mid(0, n);
}

PkString PkString::right(int n) const
{
    const std::size_t sz = _cbuf().size();
    if (static_cast<unsigned>(n) >= static_cast<unsigned>(sz)) {
        return *this;
    }
    return mid(static_cast<int>(sz) - n, n);
}

PkString PkString::mid(int pos, int n) const
{
    const int len = size();
    int position = pos;
    int length = n;

    if (position > len) {
        return PkString();
    }
    if (position < 0) {
        if (length < 0 || length + position >= len) {
            position = 0;
            length = len;
        } else if (length + position <= 0) {
            return PkString();
        } else {
            length += position;
            position = 0;
        }
    } else if (static_cast<unsigned>(length) > static_cast<unsigned>(len - position)) {
        length = len - position;
    }

    if (length <= 0) {
        return PkString();
    }

    const std::vector<char16_t>& b = _cbuf();
    PkString r;
    r._data().assign(b.begin() + position, b.begin() + position + length);
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

PkString PkString::toLower() const
{
    std::vector<char16_t> output;
    if (!pkConvertCase(_cbuf(), output, PkUnicodeCaseData::kLowerMappings,
                       PkUnicodeCaseData::kLowerValues)) {
        return *this;
    }
    PkString result;
    result._data() = std::move(output);
    return result;
}

PkString PkString::toUpper() const
{
    std::vector<char16_t> output;
    if (!pkConvertCase(_cbuf(), output, PkUnicodeCaseData::kUpperMappings,
                       PkUnicodeCaseData::kUpperValues)) {
        return *this;
    }
    PkString result;
    result._data() = std::move(output);
    return result;
}
