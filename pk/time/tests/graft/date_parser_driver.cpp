// R-16 Task 3：依赖墙 driver——不是 libs/metadata/kis_meta_data_parser.cc。
//
// 真实的 `DateParser::parse`（`libs/metadata/kis_meta_data_parser.cc:34-48`）
// 长这样（源码逐字，来自 .superpowers/sdd/R-16/task-3-context.md）：
//
//     Value DateParser::parse(const QString& _v) const
//     {
//         if (_v.length() <= 4) {
//             return Value(QDateTime::fromString(_v, "yyyy"));
//         } else if (_v.length() <= 7) {
//             return Value(QDateTime::fromString(_v, "yyyy-MM"));
//         } else if (_v.length() <= 10) {
//             return Value(QDateTime::fromString(_v, "yyyy-MM-dd"));
//         } else if (_v.length() <= 16) {
//             return Value(QDateTime::fromString(_v, "yyyy-MM-ddThh:mm"));
//         } else if (_v.length() <= 19) {
//             return Value(QDateTime::fromString(_v, "yyyy-MM-ddThh:mm:ss"));
//         } else {
//             return Value(QDateTime::fromString(_v));
//         }
//     }
//
// 这个文件复刻的是它的"长度判断链调用形状"（六分支：<=4/<=7/<=10/<=16/
// <=19/else），把 `QDateTime::fromString` 换成 `PkDateTime` 对应的静态方法、
// 返回值不再包一层 `Value(...)`——真实文件整体耦合在 `Value`/`QVariant` 与
// `libs/metadata` 的 schema 静态注册表里（`DateParser` 只是
// `KisMetaData::TypeInfo` 体系登记的众多 `Parser` 子类之一），这整套不在
// `pk/time` 的 `locks` 范围内，剥它是后续把 `libs/metadata` 剥 Qt 的任务的事
// （不是本任务，本任务只交付 `pk/time` 这一个薄壳库）。
//
// 依赖墙 driver 的四条降级路径证据（写进 task-3-report.md，这里只留指针）：
//   1. 本文件与真实 DateParser::parse 逐分支同构，长度阈值（4/7/10/16/19）与
//      六个格式串（"yyyy"/"yyyy-MM"/"yyyy-MM-dd"/"yyyy-MM-ddThh:mm"/
//      "yyyy-MM-ddThh:mm:ss"/无格式）逐字照抄源码。
//   2. 下面每个用例的期望输出来自探针实测（task-3-context.md 的 fromString
//      系列用例），不是臆造值。
//   3. 本文件顶部本注释已说明"这不是 kis_meta_data_parser.cc"。
//   4. 挡住的墙是 libs/metadata 模块的构建产物（Value/QVariant 桥接 +
//      TypeInfo/Parser schema 静态注册表），归后续把 libs/metadata 剥 Qt 的
//      任务拆掉，不在本任务范围。
//
// 跑法：不接 pk/test harness——六个分支各自只是"选对格式串、调用
// PkDateTime::fromString、核对期望输出"的直白断言，接 QObject/moc 风格的
// harness 纯属额外开销，一个独立 main() + 显式 if 判断更简单可靠（见
// task-3-report.md「driver 怎么接构建」一节的取舍说明）。

#include "../../PkDateTime.h"

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void expectTrue(bool cond, const char *what)
{
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("PASS: %s\n", what);
    }
}

// 逐分支复刻 DateParser::parse 的长度判断链——六分支，阈值与格式串跟源码
// 逐字对应。返回值直接是 PkDateTime（真实函数还包一层 Value(...)，那一层
// 依赖 libs/metadata 的 Value/QVariant 桥接，不在 pk/time 范围，本 driver
// 不复刻）。
PkDateTime parseLikeDateParser(const std::string &v)
{
    if (v.length() <= 4) {
        return PkDateTime::fromString(v, "yyyy");
    } else if (v.length() <= 7) {
        return PkDateTime::fromString(v, "yyyy-MM");
    } else if (v.length() <= 10) {
        return PkDateTime::fromString(v, "yyyy-MM-dd");
    } else if (v.length() <= 16) {
        return PkDateTime::fromString(v, "yyyy-MM-ddThh:mm");
    } else if (v.length() <= 19) {
        return PkDateTime::fromString(v, "yyyy-MM-ddThh:mm:ss");
    } else {
        return PkDateTime::fromString(v); // else 分支：无格式兜底，不是又一个 customFormat
    }
}

} // namespace

int main()
{
    // 分支 1：<=4 → "yyyy"。探针：fromString("2024","yyyy") toString(ISODate)
    // => 2024-01-01T00:00:00。
    {
        const PkDateTime dt = parseLikeDateParser("2024");
        expectTrue(dt.isValid(), "分支<=4(\"2024\") isValid");
        expectTrue(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-01-01T00:00:00",
                   "分支<=4(\"2024\") toString(ISODate) == 2024-01-01T00:00:00");
    }

    // 分支 2：<=7 → "yyyy-MM"。探针：fromString("2024-03","yyyy-MM")
    // toString(ISODate) => 2024-03-01T00:00:00。
    {
        const PkDateTime dt = parseLikeDateParser("2024-03");
        expectTrue(dt.isValid(), "分支<=7(\"2024-03\") isValid");
        expectTrue(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-03-01T00:00:00",
                   "分支<=7(\"2024-03\") toString(ISODate) == 2024-03-01T00:00:00");
    }

    // 分支 3：<=10 → "yyyy-MM-dd"。探针：fromString("2024-03-15","yyyy-MM-dd")
    // toString(ISODate) => 2024-03-15T00:00:00。
    {
        const PkDateTime dt = parseLikeDateParser("2024-03-15");
        expectTrue(dt.isValid(), "分支<=10(\"2024-03-15\") isValid");
        expectTrue(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-03-15T00:00:00",
                   "分支<=10(\"2024-03-15\") toString(ISODate) == 2024-03-15T00:00:00");
    }

    // 分支 4：<=16 → "yyyy-MM-ddThh:mm"。探针：fromString(...THh:mm)
    // toString(ISODate) => 2024-03-15T08:30:00。
    {
        const PkDateTime dt = parseLikeDateParser("2024-03-15T08:30");
        expectTrue(dt.isValid(), "分支<=16(\"2024-03-15T08:30\") isValid");
        expectTrue(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-03-15T08:30:00",
                   "分支<=16(\"2024-03-15T08:30\") toString(ISODate) == 2024-03-15T08:30:00");
    }

    // 分支 5：<=19 → "yyyy-MM-ddThh:mm:ss"。探针：fromString(...THh:mm:ss)
    // toString(ISODate) => 2024-03-15T08:30:45。
    {
        const PkDateTime dt = parseLikeDateParser("2024-03-15T08:30:45");
        expectTrue(dt.isValid(), "分支<=19(\"2024-03-15T08:30:45\") isValid");
        expectTrue(dt.toString(PkDateTime::DateFormat::ISODate) == "2024-03-15T08:30:45",
                   "分支<=19(\"2024-03-15T08:30:45\") toString(ISODate) == 2024-03-15T08:30:45");
    }

    // 分支 6：else（长度 >19）→ 无格式兜底（Qt::TextDate 默认解析）。
    // "Wed May 20 03:40:13 2015" 长度 24（>19），走 else 分支。探针：
    // fromString(TextDate default) isValid => true；toString(ISODate) =>
    // 2015-05-20T03:40:13。
    {
        const std::string v = "Wed May 20 03:40:13 2015";
        expectTrue(v.length() > 19, "分支 else 的测试输入长度确实 >19（走 else，不是误入分支 5）");
        const PkDateTime dt = parseLikeDateParser(v);
        expectTrue(dt.isValid(), "分支 else(\"Wed May 20 03:40:13 2015\") isValid");
        expectTrue(dt.toString(PkDateTime::DateFormat::ISODate) == "2015-05-20T03:40:13",
                   "分支 else(\"Wed May 20 03:40:13 2015\") toString(ISODate) == 2015-05-20T03:40:13");
    }

    if (g_failures == 0) {
        std::printf("date_parser_driver: 六分支全绿\n");
        return 0;
    }
    std::printf("date_parser_driver: %d 条失败\n", g_failures);
    return 1;
}
