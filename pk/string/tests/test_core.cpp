#include "../PkString.h"
#include "test_util.h"

#include <string>
#include <utility>

void run_core_tests()
{
    PkString empty;
    _expect(empty.isEmpty(), "default-constructed string is empty");
    _expect(empty.size() == 0, "default-constructed size is 0");

    PkString hello("hello");
    _expect(!hello.isEmpty(), "hello is not empty");
    _expect(hello.size() == 5, "hello size is 5");
    _expect(hello.at(0) == u'h', "hello at(0) is h");
    _expect(hello.at(4) == u'o', "hello at(4) is o");
    _expect(hello.at(99) == u'\0', "out-of-range at() returns NUL");
    _expect(hello.at(-1) == u'\0', "negative at() returns NUL");

    // COW：拷贝后改一个，另一个不受影响
    PkString a("abc");
    PkString b = a;
    _expect(a == b, "copy equals original");
    b += PkString("d");
    _expect(a.size() == 3, "original unchanged after copy is modified");
    _expect(b.size() == 4, "copy grew");
    _expect(a != b, "copy diverged from original");

    // 非 ASCII：UTF-8 输入按 UTF-16 码元计长
    PkString cjk("中文");
    _expect(cjk.size() == 2, "two CJK chars are two UTF-16 code units");
    _expect(cjk.at(0) == u'中', "first CJK char decodes correctly");

    // 代理对：U+1F600 在 UTF-16 里占两个码元
    PkString emoji("\xF0\x9F\x98\x80");
    _expect(emoji.size() == 2, "astral char occupies two UTF-16 code units");

    // 往返：UTF-8 → UTF-16 → UTF-8 不丢信息
    _expect(PkString("中文a\xF0\x9F\x98\x80").PkToUtf8() == std::string("中文a\xF0\x9F\x98\x80"),
            "utf8 round-trip preserves CJK and astral chars");
    _expect(PkString("\xFF\xFE").size() == 2, "invalid utf8 bytes map to replacement chars");

    // 移动后的源仍可安全查询
    PkString src("moved");
    PkString dst(std::move(src));
    _expect(dst == PkString("moved"), "move constructor transfers content");
    _expect(src.size() == 0, "moved-from string is empty but usable");

    // 排序运算符
    _expect(PkString("abc") < PkString("abd"), "operator< compares by code unit");
    _expect(!(PkString("abc") < PkString("abc")), "operator< is irreflexive");
    _expect((PkString("ab") + PkString("cd")) == PkString("abcd"), "operator+ concatenates");
    _expect(hello[1] == u'e', "operator[] indexes code units");
}
