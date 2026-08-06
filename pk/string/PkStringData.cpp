#include "PkStringData.h"

namespace {

const char16_t kReplacement = 0xFFFD;

// 把一个码点按 UTF-16 追加进缓冲区；非法码点写成 U+FFFD（不中止进程）。
void pkAppendCodePoint(std::vector<char16_t>& out, unsigned cp)
{
    if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        out.push_back(kReplacement);
        return;
    }
    if (cp < 0x10000u) {
        out.push_back(static_cast<char16_t>(cp));
        return;
    }
    cp -= 0x10000u;
    out.push_back(static_cast<char16_t>(0xD800u + (cp >> 10)));
    out.push_back(static_cast<char16_t>(0xDC00u + (cp & 0x3FFu)));
}

void pkAppendUtf8(std::string& out, unsigned cp)
{
    if (cp < 0x80u) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800u) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp < 0x10000u) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

} // namespace

std::shared_ptr<PkStringData> PkStringData::PkMakeEmpty()
{
    return std::make_shared<PkStringData>();
}

std::shared_ptr<PkStringData> PkStringData::PkClone(const PkStringData& other)
{
    std::shared_ptr<PkStringData> d = std::make_shared<PkStringData>();
    d->buf = other.buf;
    return d;
}

// 手写 UTF-8 → UTF-16 解码。std::wstring_convert 在 C++17 起已废弃，不用；
// 也不引第三方库。非法序列一律映射成 U+FFFD 并前进一个字节。
std::shared_ptr<PkStringData> PkStringData::PkFromUtf8(const char* s, std::size_t len)
{
    std::shared_ptr<PkStringData> d = std::make_shared<PkStringData>();
    if (s == nullptr || len == 0) {
        return d;
    }
    d->buf.reserve(len);

    static const unsigned kMinForLength[4] = {0x0u, 0x80u, 0x800u, 0x10000u};

    std::size_t i = 0;
    while (i < len) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        unsigned cp = 0;
        int extra = 0;

        if (c < 0x80u) {
            cp = c;
            extra = 0;
        } else if ((c & 0xE0u) == 0xC0u) {
            cp = c & 0x1Fu;
            extra = 1;
        } else if ((c & 0xF0u) == 0xE0u) {
            cp = c & 0x0Fu;
            extra = 2;
        } else if ((c & 0xF8u) == 0xF0u) {
            cp = c & 0x07u;
            extra = 3;
        } else {
            d->buf.push_back(kReplacement);
            ++i;
            continue;
        }

        if (i + static_cast<std::size_t>(extra) >= len && extra > 0) {
            d->buf.push_back(kReplacement);
            ++i;
            continue;
        }

        bool wellFormed = true;
        for (int k = 1; k <= extra; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]);
            if ((cc & 0xC0u) != 0x80u) {
                wellFormed = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        if (!wellFormed) {
            d->buf.push_back(kReplacement);
            ++i;
            continue;
        }
        if (cp < kMinForLength[extra]) {
            // 过长编码（overlong）：拒绝，按一个坏字节前进
            d->buf.push_back(kReplacement);
            ++i;
            continue;
        }

        pkAppendCodePoint(d->buf, cp);
        i += static_cast<std::size_t>(extra) + 1;
    }
    return d;
}

std::string PkStringData::PkToUtf8(const std::vector<char16_t>& b)
{
    std::string out;
    out.reserve(b.size());
    for (std::size_t i = 0; i < b.size(); ++i) {
        unsigned cp = static_cast<unsigned>(b[i]);
        if (cp >= 0xD800u && cp <= 0xDBFFu && i + 1 < b.size()
            && b[i + 1] >= 0xDC00u && b[i + 1] <= 0xDFFFu) {
            cp = 0x10000u + ((cp - 0xD800u) << 10) + (static_cast<unsigned>(b[i + 1]) - 0xDC00u);
            ++i;
        } else if (cp >= 0xD800u && cp <= 0xDFFFu) {
            cp = kReplacement;  // 孤立代理项
        }
        pkAppendUtf8(out, cp);
    }
    return out;
}
