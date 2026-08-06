#include "../PkString.h"
#include "test_util.h"

#include <vector>

void run_query_tests()
{
    PkString s("hello world");

    _expect(s.contains(PkString("lo w")), "contains finds substring");
    _expect(!s.contains(PkString("xyz")), "contains rejects absent substring");
    _expect(s.contains(PkString("")), "contains empty is always true");

    _expect(s.left(5) == PkString("hello"), "left(5)");
    _expect(s.left(0).isEmpty(), "left(0) is empty");
    _expect(s.left(999) == s, "left beyond size returns whole string");

    _expect(s.right(5) == PkString("world"), "right(5)");
    _expect(s.right(0).isEmpty(), "right(0) is empty");
    _expect(s.right(999) == s, "right beyond size returns whole string");

    _expect(s.mid(6) == PkString("world"), "mid(6) to end");
    _expect(s.mid(0, 5) == PkString("hello"), "mid(0,5)");
    _expect(s.mid(6, 999) == PkString("world"), "mid with oversized length clamps");
    _expect(s.mid(99).isEmpty(), "mid past end is empty");
    _expect(s.mid(-1, 2).isEmpty(), "negative pos is empty");

    _expect(s.startsWith(PkString("hello")), "startsWith prefix");
    _expect(!s.startsWith(PkString("world")), "startsWith rejects non-prefix");
    _expect(s.startsWith(PkString("")), "startsWith empty is always true");
    _expect(!PkString("ab").startsWith(PkString("abc")), "startsWith rejects longer prefix");

    _expect(PkString("  pad  ").trimmed() == PkString("pad"), "trimmed strips spaces");
    _expect(PkString("\t\nx\r\n").trimmed() == PkString("x"), "trimmed strips all whitespace kinds");
    _expect(PkString("   ").trimmed().isEmpty(), "all-whitespace trims to empty");
    _expect(PkString("mid dle").trimmed() == PkString("mid dle"), "trimmed keeps inner spaces");

    std::vector<PkString> parts = PkString("a,b,c").split(u',');
    _expect(parts.size() == 3, "split yields 3 parts");
    _expect(parts[0] == PkString("a"), "split part 0");
    _expect(parts[2] == PkString("c"), "split part 2");

    std::vector<PkString> withEmpty = PkString("a,,b").split(u',');
    _expect(withEmpty.size() == 3, "split keeps empty segments");
    _expect(withEmpty[1].isEmpty(), "middle segment is empty");

    std::vector<PkString> none = PkString("abc").split(u',');
    _expect(none.size() == 1, "split with no separator yields one part");

    std::vector<PkString> ofEmpty = PkString("").split(u',');
    _expect(ofEmpty.size() == 1, "splitting empty string yields one empty part");

    std::vector<PkString> edges = PkString(",a,").split(u',');
    _expect(edges.size() == 3, "leading and trailing separators keep empty edge segments");
    _expect(edges[0].isEmpty() && edges[2].isEmpty(), "edge segments are empty");

    // 切片对 UTF-16 码元下标操作，代理对不做特殊处理（与 QString 一致）
    PkString astral("\xF0\x9F\x98\x80\x61");
    _expect(astral.size() == 3, "astral char plus ascii is three code units");
    _expect(astral.right(1) == PkString("a"), "right() counts code units");
    _expect(astral.mid(2, 1) == PkString("a"), "mid() counts code units");
}
