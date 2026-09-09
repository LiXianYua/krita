#include <PkString.h>

#include "../text/KoSvgTextNewlineNormalizer.h"

#include <iostream>

namespace
{
bool check(const char *label, const PkString &input, const PkString &expected)
{
    const PkString actual = normalizeSvgTextNewlines(input);
    if (actual == expected) {
        return true;
    }

    std::cerr << label << " failed: expected " << expected.size()
              << " code units, got " << actual.size() << '\n';
    return false;
}
}

int main()
{
    bool ok = true;
    ok &= check("CRLF", PkString("left\r\nright"), PkString("left\nright"));
    ok &= check("isolated CR", PkString("left\rright"), PkString("left\nright"));
    ok &= check("mixed line endings",
                PkString("a\r\nb\rc\n"),
                PkString("a\nb\nc\n"));
    return ok ? 0 : 1;
}
