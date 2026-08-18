#include "../PkString.h"
#include "test_util.h"

#include <climits>
#include <clocale>
#include <cmath>
#include <cstdio>

namespace {

// 切到一个「小数点是逗号」的 locale。系统没装任何这类 locale 就返回 nullptr。
// 注意：C++ 程序的 C locale 默认恒为 "C"，**与环境变量无关**——不显式
// setlocale 的话，toDouble/arg(double) 的 LC_NUMERIC 缺陷在测试里永远看不见。
// 而 Krita 运行时 Qt 会 setlocale(LC_ALL, "")，跑的正是另一个 locale。
const char* pkEnterCommaLocale()
{
    static const char* const kCandidates[] = {
        "de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8",
        "ru_RU.UTF-8", "es_ES.UTF-8", "it_IT.UTF-8", "nl_NL.UTF-8",
        "pt_BR.UTF-8", "tr_TR.UTF-8", "de_DE",      "fr_FR",
    };
    const std::size_t n = sizeof(kCandidates) / sizeof(kCandidates[0]);
    for (std::size_t i = 0; i < n; ++i) {
        if (std::setlocale(LC_NUMERIC, kCandidates[i]) != nullptr
            && std::localeconv()->decimal_point[0] == ',') {
            return kCandidates[i];
        }
    }
    std::setlocale(LC_NUMERIC, "C");
    return nullptr;
}

} // namespace

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

    // 三参形式（真实调用点：kis_assert.cpp、KoFFWWSConverter.cpp）
    _expect(PkString("%1-%2-%3").arg(PkString("a"), PkString("b"), PkString("c")) == PkString("a-b-c"),
            "three-argument arg substitutes by placeholder position");
    _expect(PkString("%3-%1-%2").arg(PkString("a"), PkString("b"), PkString("c")) == PkString("c-a-b"),
            "three-argument arg honors out-of-order placeholder numbers");
    _expect(PkString("%1-%1-%2").arg(PkString("a"), PkString("b"), PkString("c")) == PkString("a-a-b"),
            "three-argument arg fills all occurrences of a repeated placeholder from the same arg");
    _expect(PkString("ASSERT failure in %1: \"%2\" (%3)")
                    .arg(PkString("where"), PkString("what"), PkString("assertion"))
                == PkString("ASSERT failure in where: \"what\" (assertion)"),
            "three-argument arg matches the real kis_assert.cpp call site shape");
    _expect(PkString("%1-%2-%3").arg(PkString("%2"), PkString("x"), PkString("y")) == PkString("%2-x-y"),
            "three-argument arg does not rescan substituted text within the same call");
    _expect(PkString("%0-%1-%2").arg(PkString("a"), PkString("b"), PkString("c")) == PkString("a-b-c"),
            "three-argument arg still honors %0 as a valid placeholder");

    // 数值重载
    _expect(PkString("n=%1").arg(42) == PkString("n=42"), "arg(int)");
    _expect(PkString("n=%1").arg(-7) == PkString("n=-7"), "arg(int) handles negatives");
    _expect(PkString("d=%1").arg(1.5) == PkString("d=1.5"), "arg(double)");
    // arg(double) 必须与 C locale 下的 printf "%g"（精度 6）逐字一致
    _expect(PkString("%1").arg(1.0) == PkString("1"), "arg(double) drops a trailing .0 like %g");
    _expect(PkString("%1").arg(0.0001) == PkString("0.0001"), "arg(double) keeps small decimals like %g");
    _expect(PkString("%1").arg(1000000.0) == PkString("1e+06"), "arg(double) switches to exponent like %g");

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
    _expect(PkString("+3.5").toDouble(&ok) == 3.5, "toDouble tolerates a leading plus");
    // strtod 家族会把这些当十六进制浮点吃掉，QString::toDouble 不接受十六进制。
    // 解析实现换来换去时这两条最容易被悄悄放行，所以钉死。
    _expect(PkString("0x10").toDouble(&ok) == 0.0, "toDouble rejects hex literals");
    _expect(!ok, "hex input sets ok=false");
    _expect(PkString("0x1p3").toDouble(&ok) == 0.0, "toDouble rejects hex float literals");
    _expect(!ok, "hex float input sets ok=false");
    _expect(PkString("-0X2").toDouble(&ok) == 0.0, "toDouble rejects signed uppercase hex");
    _expect(!ok, "signed hex input sets ok=false");
    // 十六进制闸门必须建在 strtod 真正开始消费的位置上。若在外面先剥掉 '+'，
    // strtod 会重开一轮「跳空白 + 认符号」，下面这些就会从闸门底下溜过去。
    _expect(PkString("+ 0x10").toDouble(&ok) == 0.0, "hex after sign-then-space is still rejected");
    _expect(!ok, "sign-space-hex sets ok=false");
    _expect(PkString("+\t0x1p3").toDouble(&ok) == 0.0, "hex after sign-then-tab is still rejected");
    _expect(!ok, "sign-tab-hex sets ok=false");
    // 同根因：C 的 strtod 文法是「空白 → 一个符号 → 数字」，符号后不许再有
    // 空白或第二个符号。QString 同样不接受，这几条都必须失败。
    _expect(PkString("++3.5").toDouble(&ok) == 0.0, "a doubled sign is not a number");
    _expect(!ok, "doubled sign sets ok=false");
    _expect(PkString("+ 3.5").toDouble(&ok) == 0.0, "whitespace after the sign is not a number");
    _expect(!ok, "sign-then-space sets ok=false");
    _expect(PkString("+  -2.5").toDouble(&ok) == 0.0, "sign, spaces, then another sign is not a number");
    _expect(!ok, "sign-spaces-sign sets ok=false");
    _expect(PkString("+\n7").toDouble(&ok) == 0.0, "a newline after the sign is not a number");
    _expect(!ok, "sign-then-newline sets ok=false");

    // 次正规数（渐进下溢）：glibc 的 strtod 会置 ERANGE，但那是**成功**的解析。
    // 把 ERANGE 一刀切当失败会把这两个真值误杀成 0/false。边界卡在 DBL_MIN 上。
    _expect(PkString("1e-310").toDouble(&ok) != 0.0, "a subnormal parses to a non-zero value");
    _expect(ok, "gradual underflow to a subnormal is a success, not a failure");
    _expect(PkString("4.9e-324").toDouble(&ok) != 0.0, "the smallest subnormal still parses");
    _expect(ok, "denormal-min sets ok=true");
    // 两头才是真失败：全下溢（返回 0）与上溢（返回 ±inf）。
    _expect(PkString("1e-400").toDouble(&ok) == 0.0, "total underflow is a failure");
    _expect(!ok, "underflow to zero sets ok=false");
    // 上溢 ok=false，但真实 Qt 返回算出来的 +infinity，不是 0.0（背景 ⑦a，
    // 详细覆盖见文件末尾的 inf/nan 测试块）——这条只钉住 ok，不再断言 ==0.0。
    _expect(std::isinf(PkString("1e400").toDouble(&ok)), "overflow still parses to infinity, not 0.0");
    _expect(!ok, "overflow to infinity sets ok=false");
    // 合法的零不会置 ERANGE，不能被上面的规则连累
    _expect(PkString("0").toDouble(&ok) == 0.0, "a literal zero parses");
    _expect(ok, "a literal zero sets ok=true");
    _expect(PkString("0.0").toDouble(&ok) == 0.0, "a literal 0.0 parses");
    _expect(ok, "a literal 0.0 sets ok=true");
    _expect(PkString("-0.0").toDouble(&ok) == 0.0, "a literal -0.0 parses");
    _expect(ok, "a literal -0.0 sets ok=true");
    // 串里嵌了 U+0000 时，NUL 之后的垃圾必须照样被拒——解析函数在 NUL 处停下，
    // 但尾随判定要按真实长度算，不能按 C 串长度算。
    _expect(PkString::PkFromUtf8("1.5\0xx", 6).toDouble(&ok) == 0.0,
            "toDouble rejects garbage after an embedded NUL");
    _expect(!ok, "embedded-NUL garbage sets ok=false");
    _expect(PkString::PkFromUtf8("12\0zz", 5).toInt(&ok) == 0,
            "toInt rejects garbage after an embedded NUL");
    _expect(!ok, "embedded-NUL garbage sets toInt ok=false");
    _expect(PkString(" 2.5 ").toDouble(&ok) == 2.5, "toDouble tolerates surrounding whitespace");
    _expect(PkString("+8").toInt(&ok) == 8, "toInt tolerates a leading plus");
    // toInt 的符号文法必须和 toDouble 一样严：空白 → 一个符号 → 数字。
    // 整数 from_chars 不认 '+' 但**认 '-'**，所以只在外面剥掉一个 '+' 是不够的：
    // "+-3" 剩下 "-3" 会被正常解析成 -3。QString 这些全是 0/false。
    _expect(PkString("+-3").toInt(&ok) == 0, "toInt rejects a plus followed by a minus");
    _expect(!ok, "plus-minus sets toInt ok=false");
    _expect(PkString("+-0").toInt(&ok) == 0, "toInt rejects plus-minus even when the digits are zero");
    _expect(!ok, "plus-minus-zero sets toInt ok=false");
    _expect(PkString(" +-3").toInt(&ok) == 0, "leading blanks do not excuse a double sign");
    _expect(!ok, "blank-plus-minus sets toInt ok=false");
    _expect(PkString("-+3").toInt(&ok) == 0, "toInt rejects a minus followed by a plus");
    _expect(!ok, "minus-plus sets toInt ok=false");
    _expect(PkString("++3").toInt(&ok) == 0, "toInt rejects a doubled plus");
    _expect(!ok, "doubled plus sets toInt ok=false");
    _expect(PkString("--3").toInt(&ok) == 0, "toInt rejects a doubled minus");
    _expect(!ok, "doubled minus sets toInt ok=false");
    _expect(PkString("+ 3").toInt(&ok) == 0, "toInt rejects whitespace between sign and digits");
    _expect(!ok, "sign-space-digit sets toInt ok=false");
    _expect(PkString("+").toInt(&ok) == 0, "a lone sign is not a number");
    _expect(!ok, "a lone sign sets toInt ok=false");
    // 负数要连符号一起交给 from_chars，自己取负会在 INT_MIN 上溢出
    _expect(PkString("-2147483648").toInt(&ok) == -2147483648, "toInt parses INT_MIN");
    _expect(ok, "INT_MIN sets ok=true");
    _expect(PkString("+2147483647").toInt(&ok) == 2147483647, "toInt parses INT_MAX with a plus");
    _expect(ok, "INT_MAX sets ok=true");
    // 对称：toDouble 侧的同一组也钉住（strtod 自己就拦，但别让它悄悄退化）
    _expect(PkString("+-3.5").toDouble(&ok) == 0.0, "toDouble rejects a plus followed by a minus");
    _expect(!ok, "plus-minus sets toDouble ok=false");
    _expect(PkString("-+3.5").toDouble(&ok) == 0.0, "toDouble rejects a minus followed by a plus");
    _expect(!ok, "minus-plus sets toDouble ok=false");
    _expect(PkString("--3.5").toDouble(&ok) == 0.0, "toDouble rejects a doubled minus");
    _expect(!ok, "doubled minus sets toDouble ok=false");
    _expect(PkString("99999999999999999999").toInt(&ok) == 0, "toInt rejects out-of-range values");
    _expect(!ok, "out-of-range toInt sets ok=false");

    // ── LC_NUMERIC 免疫 ───────────────────────────────────────────
    // QString::toDouble / QString::arg(double) 硬编码 C locale
    // （Qt 内部走 QLocaleData::c()），不受进程全局 locale 影响。
    // strtod / snprintf("%g") 则相反：小数点字符由 LC_NUMERIC 决定。
    // Krita 运行时 Qt 的 initLocale 会 setlocale(LC_ALL, "")，所以德语等
    // 系统上这个差异是**会真实触发**的：曾经的实现在 de_DE 下把
    // toDouble("0.75") 解析成 0（ok=false）、把 arg(0.75) 输出成 "0,75"，
    // 而 kis_properties_configuration.cc 与 kis_predefined_brush_factory.cpp
    // 正是靠 toDouble 读预设/笔刷参数的 —— 会静默归零。
    const char* commaLocale = pkEnterCommaLocale();
    if (commaLocale != nullptr) {
        bool lok = false;
        _expect(PkString("0.75").toDouble(&lok) == 0.75,
                "toDouble ignores LC_NUMERIC under a comma-decimal locale");
        _expect(lok, "toDouble sets ok=true under a comma-decimal locale");
        _expect(PkString("-0.25").toDouble(&lok) == -0.25,
                "toDouble parses negatives under a comma-decimal locale");
        _expect(PkString("%1").arg(0.75) == PkString("0.75"),
                "arg(double) emits a dot under a comma-decimal locale");
        _expect(PkString("0,75").toDouble(&lok) == 0.0,
                "a comma is never a decimal separator, not even in a comma locale");
        _expect(!lok, "comma-separated input sets ok=false");
        _expect(PkString("123").toInt(&lok) == 123, "toInt is unaffected by LC_NUMERIC");
        _expect(PkString("n=%1").arg(42) == PkString("n=42"), "arg(int) is unaffected by LC_NUMERIC");
        std::setlocale(LC_NUMERIC, "C");
    } else {
        // 这一组是本文件里最强的一项检查。没跑到就必须看得见——
        // 「判据指着一个不存在的东西时它不报错，它放行」正是要避免的失效方式。
        std::printf("NOTE: 本机没装小数点为逗号的 locale，LC_NUMERIC 免疫检查未跑到。\n");
        std::printf("      本机造一个（无需 sudo）：\n");
        std::printf("        localedef -i de_DE -f UTF-8 <dir>/de_DE.UTF-8\n");
        std::printf("        LOCPATH=<dir> LC_ALL=de_DE.UTF-8 ./test_pkstring\n");
    }

    // %0：占位符编号从 0 开始
    _expect(PkString("%0-%1").arg(PkString("x")) == PkString("x-%1"),
            "arg fills placeholder %0, leaving %1 untouched with only one arg");

    // %L：locale 千分位分组，逐位置独立生效
    _expect(PkString("%L1").arg(1234567) == PkString("1,234,567"),
            "%L1 groups a large int with commas");
    _expect(PkString("%L1").arg(999) == PkString("999"),
            "%L1 does not group a value under 1000");
    _expect(PkString("%L1").arg(1000) == PkString("1,000"),
            "%L1 groups exactly at the 1000 boundary");
    _expect(PkString("%L1").arg(-1234567) == PkString("-1,234,567"),
            "%L1 groups negatives without grouping the sign");
    _expect(PkString("%L1 %1").arg(1234567) == PkString("1,234,567 1234567"),
            "the same int arg is grouped at %L1 but not at plain %1");
    _expect(PkString("%L2 %2").arg(PkString("a")).arg(PkString("b")) == PkString("a a"),
            "%L has no effect when the substituted arg is a string, not a number");

    // arg(int, int fieldWidth)：真实调用点 libs/global/KisRectsGrid.cpp:23 的形态
    _expect(PkString("grid=%1").arg(3, 6) == PkString("grid=     3"),
            "arg(int,fieldWidth) right-justifies with spaces for positive width");
    _expect(PkString("grid=%1").arg(3, -6) == PkString("grid=3     "),
            "arg(int,fieldWidth) left-justifies for negative width");
    _expect(PkString("grid=%1").arg(3, 0) == PkString("grid=3"),
            "arg(int,fieldWidth) with width 0 pads nothing");
    _expect(PkString("grid=%1").arg(-3, 6) == PkString("grid=    -3"),
            "arg(int,fieldWidth) counts the sign toward the field width");

    // arg(int, fieldWidth) 遇 %L1：先分组再按分组后长度补宽度（真实 Qt 5.15.7 实测）
    _expect(PkString("[%L1]").arg(1234567, 12) == PkString("[   1,234,567]"),
            "arg(int,fieldWidth) groups then pads to the grouped length for %L1");
    _expect(PkString("[%L1]").arg(1234567, 6) == PkString("[1,234,567]"),
            "arg(int,fieldWidth) does not truncate when the grouped string already exceeds fieldWidth");
    _expect(PkString("[%L1]").arg(1234567, -12) == PkString("[1,234,567   ]"),
            "arg(int,fieldWidth) left-justifies the grouped string for negative fieldWidth");
    _expect(PkString("[%L1]").arg(999, 12) == PkString("[         999]"),
            "arg(int,fieldWidth) pads the ungrouped string when the value is under 1000");
    _expect(PkString("[%L1]").arg(-1234567, 14) == PkString("[    -1,234,567]"),
            "arg(int,fieldWidth) groups negatives without grouping the sign, sign counts toward width");

    // R-13 最终评审 I5：fieldWidth==INT_MIN 时 -fieldWidth 是有符号整数溢出，
    // 曾经会让 vector 补齐操作抛 std::length_error 崩溃。真实 Qt 在这个极端输入
    // 下正常返回，不深究具体数值，只钉住"不崩溃、返回值是原始数字串这个层面
    // 上合理"这条弱断言。
    {
        PkString r = PkString("[%1]").arg(3, INT_MIN);
        _expect(!r.isEmpty(), "arg(int,fieldWidth) with fieldWidth==INT_MIN does not crash");
        _expect(r.contains(PkString("3")), "arg(int,fieldWidth) with fieldWidth==INT_MIN still contains the digit");
    }

    // R-13 最终评审 C1(a)/C1(b)：arg(double) 的负零与 %L 分组
    _expect(PkString("%1").arg(-0.0) == PkString("0"),
            "arg(double) drops the sign of negative zero, matching real Qt");
    _expect(PkString("%1").arg(0.0) == PkString("0"), "arg(double) of positive zero is unaffected");
    _expect(PkString("[%L1]").arg(1234.5) == PkString("[1,234.5]"),
            "arg(double) %L groups only the integer part, decimal part untouched");
    _expect(PkString("[%L1]").arg(123456.0) == PkString("[123,456]"),
            "arg(double) %L groups a large integral value");
    _expect(PkString("[%L1]").arg(1000.0) == PkString("[1,000]"),
            "arg(double) %L groups exactly at the 1000 boundary");
    _expect(PkString("[%L1]").arg(-1234.5) == PkString("[-1,234.5]"),
            "arg(double) %L groups negatives without grouping the sign");
    _expect(PkString("[%L1]").arg(999.5) == PkString("[999.5]"),
            "arg(double) %L does not group when the integer part is under 1000");
    _expect(PkString("[%L1]").arg(1000000.0) == PkString("[1e+06]"),
            "arg(double) %L does not group when the result is in scientific notation");

    // toDouble：inf/nan 的窄口径
    {
        bool iok = false;
        _expect(PkString("inf").toDouble(&iok) > 0 && std::isinf(PkString("inf").toDouble()), "toDouble(\"inf\") parses to +infinity");
        _expect(iok, "toDouble(\"inf\") sets ok=true");
        _expect(PkString("Inf").toDouble(&iok) > 0, "toDouble is case-insensitive for inf");
        _expect(iok, "\"Inf\" sets ok=true");
        _expect(PkString("-inf").toDouble(&iok) < 0, "toDouble(\"-inf\") parses to -infinity");
        _expect(iok, "\"-inf\" sets ok=true");
        _expect(PkString("nan").toDouble(&iok) != PkString("nan").toDouble(&iok), "toDouble(\"nan\") parses to NaN (NaN != NaN)");
        _expect(iok, "toDouble(\"nan\") sets ok=true");

        _expect(PkString("infinity").toDouble(&iok) == 0.0, "toDouble rejects the full word \"infinity\"");
        _expect(!iok, "\"infinity\" sets ok=false");
        _expect(PkString("Infinity").toDouble(&iok) == 0.0, "toDouble rejects \"Infinity\" case-insensitively");
        _expect(!iok, "\"Infinity\" sets ok=false");
        _expect(PkString("+nan").toDouble(&iok) == 0.0, "toDouble rejects a signed \"+nan\"");
        _expect(!iok, "\"+nan\" sets ok=false");
        _expect(PkString("-nan").toDouble(&iok) == 0.0, "toDouble rejects a signed \"-nan\"");
        _expect(!iok, "\"-nan\" sets ok=false");
        _expect(PkString("nano").toDouble(&iok) == 0.0, "toDouble rejects \"nano\" (not a bare nan)");
        _expect(!iok, "\"nano\" sets ok=false");

        // 背景 ⑦a：上溢失败要带回真实的 ±inf，不能一律清零
        double ov = PkString("1e400").toDouble(&iok);
        _expect(std::isinf(ov) && ov > 0, "overflow failure still returns +infinity, not 0.0");
        _expect(!iok, "overflow sets ok=false");
        double ovNeg = PkString("-1e400").toDouble(&iok);
        _expect(std::isinf(ovNeg) && ovNeg < 0, "negative overflow failure returns -infinity");
        _expect(!iok, "negative overflow sets ok=false");
        double ov2 = PkString("1e309").toDouble(&iok);
        _expect(std::isinf(ov2) && ov2 > 0, "a mundane over-DBL_MAX overflow also returns +infinity");
        _expect(!iok, "1e309 sets ok=false");
        // 对照组：非上溢的失败路径仍然清零，不要因为改了上溢分支就连累这些
        _expect(PkString("1.5x").toDouble(&iok) == 0.0, "trailing-garbage failure still returns 0.0");
        _expect(!iok, "trailing garbage sets ok=false");
        _expect(PkString("1e-400").toDouble(&iok) == 0.0, "total-underflow failure still returns 0.0");
        _expect(!iok, "total underflow sets ok=false");
    }
}
