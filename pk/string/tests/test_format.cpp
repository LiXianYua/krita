#include "../PkString.h"
#include "test_util.h"

void run_format_tests()
{
    PkString base("ab");
    base.append(PkString("cd"));
    _expect(base == PkString("abcd"), "append concatenates in place");
    _expect(base.size() == 4, "append updates size");

    PkString acc("x");
    acc.append(PkString("")).append(PkString("y"));
    _expect(acc == PkString("xy"), "append returns *this for chaining");

    // COW：append 到副本不影响原串
    PkString orig("keep");
    PkString copy = orig;
    copy.append(PkString("!"));
    _expect(orig == PkString("keep"), "append on a copy leaves the original alone");

    // arg 基本替换
    _expect(PkString("%1 world").arg(PkString("hello")) == PkString("hello world"),
            "arg replaces %1");
    _expect(PkString("%1-%2").arg(PkString("a")).arg(PkString("b")) == PkString("a-b"),
            "chained arg fills %1 then %2");
    _expect(PkString("%2-%1").arg(PkString("a")).arg(PkString("b")) == PkString("b-a"),
            "arg fills lowest-numbered placeholder first, not left-to-right");
    _expect(PkString("%1 %1").arg(PkString("z")) == PkString("z z"),
            "arg replaces all occurrences of the same placeholder");

    // 关键：非编号的 % 不是占位符（KoProgressProxy 的 "%p%" 靠这条）
    _expect(PkString("%1: %p%").arg(PkString("job")) == PkString("job: %p%"),
            "arg leaves non-numeric % sequences untouched");
    _expect(PkString("no placeholder").arg(PkString("x")) == PkString("no placeholder"),
            "arg on a string without placeholders is a no-op");
    _expect(PkString("trailing %").arg(PkString("x")) == PkString("trailing %"),
            "a bare trailing % is not a placeholder");

    // 单次 arg 之内不重扫：刚替换进去的 %2 不会在**同一次**调用里再被吃掉
    _expect(PkString("%1-%1").arg(PkString("%2")) == PkString("%2-%2"),
            "a single arg() call does not rescan what it just substituted");
    // 但**链式** arg 会重扫 —— 这是 QString 的真实行为，Qt 官方文档的例子是
    // str = "%1 %2"; str.arg("%1f").arg("Hello") 返回 "Hellof %2"。
    // 所以这里第二次 arg 把两处 %2 一起吃掉，得 "q-q"。
    // （plan 的 Task 4 断言这里应得 "%2-q"，那是「不重扫」的语义，与 QString 不符；
    //   以 QString 为准，差异已记进 .exec/progress/R-01.md 上报。）
    _expect(PkString("%1-%2").arg(PkString("%2")).arg(PkString("q")) == PkString("q-q"),
            "chained arg rescans substituted text, exactly like QString");

    // 两位数编号
    _expect(PkString("%10/%2").arg(PkString("a")).arg(PkString("b")) == PkString("b/a"),
            "two-digit placeholders are parsed as one number");

    // 双参形式（KritaVersionWrapper 用的就是这个）
    _expect(PkString("%1 (git %2)").arg(PkString("6.0.3"), PkString("deadbeef"))
                == PkString("6.0.3 (git deadbeef)"),
            "two-argument arg fills %1 and %2");
    _expect(PkString("%1|%2").arg(PkString("%2"), PkString("z")) == PkString("%2|z"),
            "two-argument arg substitutes simultaneously, not sequentially");

    // 数值重载
    _expect(PkString("n=%1").arg(42) == PkString("n=42"), "arg(int)");
    _expect(PkString("n=%1").arg(-7) == PkString("n=-7"), "arg(int) handles negatives");
    _expect(PkString("d=%1").arg(1.5) == PkString("d=1.5"), "arg(double)");

    // toInt / toDouble
    bool ok = false;
    _expect(PkString("123").toInt(&ok) == 123, "toInt parses digits");
    _expect(ok, "toInt sets ok=true on success");
    _expect(PkString("-45").toInt(&ok) == -45, "toInt parses negative");
    _expect(PkString("abc").toInt(&ok) == 0, "toInt returns 0 on garbage");
    _expect(!ok, "toInt sets ok=false on failure");
    _expect(PkString("12abc").toInt(&ok) == 0, "toInt rejects trailing garbage");
    _expect(!ok, "toInt sets ok=false on trailing garbage");
    _expect(PkString("").toInt(&ok) == 0, "toInt on empty is 0");
    _expect(!ok, "toInt on empty sets ok=false");
    _expect(PkString("  7 ").toInt(&ok) == 7, "toInt tolerates surrounding whitespace");
    _expect(ok, "toInt whitespace case sets ok=true");
    _expect(PkString("42").toInt() == 42, "toInt works with null ok pointer");

    _expect(PkString("1.5").toDouble(&ok) == 1.5, "toDouble parses decimal");
    _expect(ok, "toDouble sets ok=true on success");
    _expect(PkString("bad").toDouble(&ok) == 0.0, "toDouble returns 0 on garbage");
    _expect(!ok, "toDouble sets ok=false on failure");
    _expect(PkString("-0.25").toDouble(&ok) == -0.25, "toDouble parses negative decimal");
    _expect(ok, "toDouble negative case sets ok=true");
    _expect(PkString("1.5x").toDouble(&ok) == 0.0, "toDouble rejects trailing garbage");
    _expect(!ok, "toDouble sets ok=false on trailing garbage");
    _expect(PkString("2.25").toDouble() == 2.25, "toDouble works with null ok pointer");
}
