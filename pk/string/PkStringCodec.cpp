#include "PkStringCodec.h"

namespace PkStringCodec {
namespace {

const char16_t kReplacement = 0xFFFD;

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

std::vector<char16_t> FromUtf8(const char* s, std::size_t len)
{
    std::vector<char16_t> buf;
    if (s == nullptr || len == 0) {
        return buf;
    }
    buf.reserve(len);

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
            buf.push_back(kReplacement);
            ++i;
            continue;
        }

        if (i + static_cast<std::size_t>(extra) >= len && extra > 0) {
            buf.push_back(kReplacement);
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
            buf.push_back(kReplacement);
            ++i;
            continue;
        }
        if (cp < kMinForLength[extra]) {
            buf.push_back(kReplacement);
            ++i;
            continue;
        }

        pkAppendCodePoint(buf, cp);
        i += static_cast<std::size_t>(extra) + 1;
    }
    return buf;
}

std::string ToUtf8(const std::vector<char16_t>& b)
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
            cp = kReplacement;  // 孤立代理项——Task 2 会把这一分支改成单字节 0x3F
        }
        pkAppendUtf8(out, cp);
    }
    return out;
}

} // namespace PkStringCodec
