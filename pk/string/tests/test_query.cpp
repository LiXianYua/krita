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
    _expect(s.mid(-1, 2) == PkString("h"), "negative pos shifts length, does not fail outright");
    _expect(s.mid(-1) == s, "negative pos with default n (-1) returns the whole string");
    _expect(s.mid(-100, 2).isEmpty(), "negative pos overshooting the whole length is empty");

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

    // left/right 对负数 n 的处理：无符号比较，任何负数都落进"返回整串"分支
    _expect(s.left(-1) == s, "left(-1) returns the whole string, not empty");
    _expect(s.left(-100) == s, "any negative n for left() returns the whole string");
    _expect(s.right(-1) == s, "right(-1) returns the whole string, not empty");
    _expect(s.right(-100) == s, "any negative n for right() returns the whole string");

    // trimmed() 剥离完整的 Unicode White_Space 集合，不只是 ASCII + NBSP
    _expect(PkString("\xC2\xA0hi\xC2\xA0").trimmed() == PkString("hi"),
            "trimmed strips NBSP (U+00A0)");
    _expect(PkString("\xE2\x80\x82hi").trimmed() == PkString("hi"),
            "trimmed strips en-space (U+2002)");
    _expect(PkString("\xE3\x80\x80hi\xE3\x80\x80").trimmed() == PkString("hi"),
            "trimmed strips ideographic space (U+3000)");
    _expect(PkString("\xC2\x85hi").trimmed() == PkString("hi"),
            "trimmed strips NEL (U+0085)");

    // 孤立代理项编码：单字节 0x3F，不是三字节 U+FFFD
    {
        // 高代理项单独存在（U+D83D，没有紧跟的低代理）
        // PkString 没有从 u16string 直接构造的公开 API（不在用量表内），
        // 用 mid() 从一个真实代理对里切出半个来构造这个场景，与 PkStringData
        // 迁移前 test_query.cpp:58-61 的 astral 用例是同一手法。
        PkString astral_test("\xF0\x9F\x98\x80");  // U+1F600，UTF-16 两个码元 D83D DE00
        _expect(astral_test.left(1).PkToUtf8() == std::string("\x3F"),
                "encoding a lone high surrogate (via left() splitting a pair) emits single-byte 0x3F");
        _expect(astral_test.mid(1, 1).PkToUtf8() == std::string("\x3F"),
                "encoding a lone low surrogate (via mid() splitting a pair) emits single-byte 0x3F");
    }

    // Unicode 13.0 default case conversion.  These are full-string mappings,
    // not byte-wise std::tolower/std::toupper and not locale-tailored rules.
    {
        const PkString mixed("Hello WORLD");
        _expect(mixed.toLower() == PkString("hello world"),
                "toLower converts mixed ASCII case");
        _expect(mixed.toUpper() == PkString("HELLO WORLD"),
                "toUpper converts mixed ASCII case");

        _expect(PkString("\xC3\x84\xCE\xA9\xD0\x96").toLower()
                    == PkString("\xC3\xA4\xCF\x89\xD0\xB6"),
                "toLower converts BMP Latin, Greek, and Cyrillic letters");
        _expect(PkString("\xC3\xA4\xCF\x89\xD0\xB6").toUpper()
                    == PkString("\xC3\x84\xCE\xA9\xD0\x96"),
                "toUpper converts BMP Latin, Greek, and Cyrillic letters");

        // U+10400 DESERET CAPITAL LETTER LONG I / U+10428 small counterpart.
        _expect(PkString("\xF0\x90\x90\x80\xF0\x90\x90\xA8").toLower()
                    == PkString("\xF0\x90\x90\xA8\xF0\x90\x90\xA8"),
                "toLower decodes and maps supplementary-plane surrogate pairs");
        _expect(PkString("\xF0\x90\x90\x80\xF0\x90\x90\xA8").toUpper()
                    == PkString("\xF0\x90\x90\x80\xF0\x90\x90\x80"),
                "toUpper decodes and maps supplementary-plane surrogate pairs");

        _expect(PkString("Stra\xC3\x9F" "e").toUpper() == PkString("STRASSE"),
                "toUpper applies one-to-many sharp-s mapping");
        _expect(PkString("\xEF\xAC\x83").toUpper() == PkString("FFI"),
                "toUpper applies one-to-many ligature mapping");
        _expect(PkString("\xC4\xB0").toLower() == PkString("i\xCC\x87"),
                "toLower applies unconditional SpecialCasing for capital dotted I");

        const PkString source("Stra\xC3\x9F" "e \xC4\xB0");
        const PkString sourceCopy = source;
        const PkString lower = source.toLower();
        const PkString upper = source.toUpper();
        _expect(source == sourceCopy, "case conversion leaves the source object unchanged");
        _expect(!lower.PkIsSharedWith(source), "changed lowercase result owns different COW data");
        _expect(!upper.PkIsSharedWith(source), "changed uppercase result owns different COW data");

        const PkString alreadyLower("already lower 123 \xCF\x89");
        const PkString alreadyUpper("ALREADY UPPER 123 \xCE\xA9");
        _expect(alreadyLower.toLower().PkIsSharedWith(alreadyLower),
                "unchanged toLower result shares COW data");
        _expect(alreadyUpper.toUpper().PkIsSharedWith(alreadyUpper),
                "unchanged toUpper result shares COW data");

        // QString's release-build case iterator consumes a non-trailing high
        // surrogate together with the following UTF-16 unit, even when that
        // unit is not a low surrogate. Pin that observable malformed-input
        // behavior with literal UTF-16 expectations.
        const PkString emoji("\xF0\x9F\x98\x80");
        const PkString loneHigh = emoji.left(1);
        const PkString loneLow = emoji.mid(1, 1);
        _expect(loneHigh.toLower().PkToU16() == std::u16string({0xD83D}),
                "toLower preserves a lone trailing high surrogate");
        _expect(loneLow.toUpper().PkToU16() == std::u16string({0xDE00}),
                "toUpper preserves a lone low surrogate");

        const PkString highBeforeUpper = loneHigh + PkString("A");
        _expect(highBeforeUpper.toLower().PkToU16() == std::u16string({0xD83D, u'A'}),
                "toLower preserves a high surrogate plus caseable uppercase BMP unit");
        const PkString highBeforeLower = loneHigh + PkString("a");
        _expect(highBeforeLower.toUpper().PkToU16() == std::u16string({0xD83D, u'a'}),
                "toUpper preserves a high surrogate plus caseable lowercase BMP unit");

        const PkString reversed = loneLow + loneHigh + PkString("A");
        _expect(reversed.toLower().PkToU16()
                    == std::u16string({0xDE00, 0xD83D, u'A'}),
                "toLower preserves a reversed pair before a caseable BMP unit");

        const PkString malformedMixed =
            PkString("A") + loneHigh + PkString("b") + loneLow + PkString("C");
        _expect(malformedMixed.toLower().PkToU16()
                    == std::u16string({u'a', 0xD83D, 0xDC62, 0xDE00, u'c'}),
                "toLower matches Qt's mixed malformed UTF-16 conversion shape");
        _expect(malformedMixed.toUpper().PkToU16()
                    == std::u16string({u'A', 0xD83D, u'b', 0xDE00, u'C'}),
                "toUpper leaves a mixed malformed UTF-16 sequence unchanged when no case mapping is seen");

        const PkString nul = PkString::PkFromUtf8("\0", 1);
        const PkString firstHigh = PkString("\xF0\x90\x80\x80").left(1); // D800
        const PkString sharpS("\xC3\x9F");
        const PkString ligatureFfi("\xEF\xAC\x83");
        struct MalformedExpansionCase {
            PkString input;
            std::u16string expected;
            const char* message;
        };
        const std::vector<MalformedExpansionCase> malformedExpansionCases = {
            {nul + nul + firstHigh + sharpS,
             {u'\0', u'\0', 0xD800, u'S', u'S'},
             "toUpper backs up to sharp-s after a first high surrogate"},
            {nul + loneHigh + sharpS,
             {u'\0', 0xD83D, u'S', u'S'},
             "toUpper backs up to sharp-s after a middle high surrogate"},
            {firstHigh + ligatureFfi,
             {0xD800, 0xFB03},
             "toUpper leaves an expansion hidden by an unmapped malformed composite"},
            {PkString("a") + firstHigh + sharpS,
             {u'A', 0x24C5, 0x00DF},
             "toUpper keeps detached write-cursor semantics after an earlier mapping"},
        };
        for (const MalformedExpansionCase& probe : malformedExpansionCases) {
            _expect(probe.input.toUpper().PkToU16() == probe.expected, probe.message);
        }
    }
}
