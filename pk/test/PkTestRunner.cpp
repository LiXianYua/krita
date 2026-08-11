#include "PkTest.h"
#include "PkTestData.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace PkTest {

namespace {

void invokeIfPresent(PkTestObject *obj, const PkTestFunction *fn)
{
    if (fn && fn->invoke) {
        fn->invoke(obj);
    }
}

// 带值开关：它们后面跟一个独立的值参数，那个值**不是**测试函数名。
// 漏掉这张表会让 `-o results.xml` 里的 results.xml 被当成过滤器，
// 于是一个测试都匹配不上 —— 静默全绿。取的是 QTest 里真正带值的那几个开关。
bool takesValue(const char *arg)
{
    static const char *const kValueOptions[] = {
        "-o", "-eventdelay", "-keydelay", "-mousedelay",
        "-maxwarnings", "-iterations", "-median", "-perfcounter",
        "-minimumvalue", "-minimumtotal",
    };
    for (const char *opt : kValueOptions) {
        if (std::strcmp(arg, opt) == 0) {
            return true;
        }
    }
    return false;
}

// QTest 的命令行：裸参数是要跑的测试函数名（可带 :dataTag）。
// R-11 只实现"按函数名过滤"这一条 —— 它是 ctest 与人工调试都会用到的，
// 其余开关（-xml/-maxwarnings/...）Krita 的测试构建没有依赖，只做参数跳过。
std::vector<std::string> parseFilters(int argc, char **argv)
{
    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i) {
        if (!argv[i]) {
            continue;
        }
        if (argv[i][0] == '-') {
            if (takesValue(argv[i])) {
                ++i;   // 连它的值一起跳过
            }
            continue;
        }
        filters.emplace_back(argv[i]);
    }
    return filters;
}

// 过滤器里的函数名必须真的存在。Qt 对未知测试函数名打印 `Unknown testfunction`
// 并以非零退出；不校验的话拼错一个名字就是"跑了 0 个测试、退出 0"，
// CI 上与真正全绿无法区分。
bool validateFilters(const std::vector<std::string> &filters,
                     const PkTestFunction *fns, int count)
{
    bool ok = true;
    for (const std::string &f : filters) {
        const std::string::size_type colon = f.find(':');
        const std::string fnName = (colon == std::string::npos) ? f : f.substr(0, colon);
        bool found = false;
        for (int i = 0; i < count; ++i) {
            if (fns[i].name && fnName == fns[i].name) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::printf("Unknown testfunction: %s\n", fnName.c_str());
            ok = false;
        }
    }
    return ok;
}

bool selected(const std::vector<std::string> &filters, const char *name)
{
    if (filters.empty()) {
        return true;
    }
    for (const std::string &f : filters) {
        const std::string::size_type colon = f.find(':');
        const std::string fnName = (colon == std::string::npos) ? f : f.substr(0, colon);
        if (fnName == name) {
            return true;
        }
    }
    return false;
}

const PkTestFunction *findByName(const PkTestFunction *fns, int count, const char *name)
{
    if (!name || !fns) {
        return nullptr;
    }
    for (int i = 0; i < count; ++i) {
        if (fns[i].name && std::strcmp(fns[i].name, name) == 0) {
            return &fns[i];
        }
    }
    return nullptr;
}

// 一次测试函数调用：数据驱动的每一行、以及非数据驱动的唯一一次，都走这里。
// dataRow 为 PkTestCase::NoDataRow 表示非数据驱动 —— beginFunction 已经把上一次的
// 行号与 tag 清掉，不用再显式设置一次。
void runOnce(PkTestObject *obj, const PkTestPlan &plan, PkTestCase &state,
            const PkTestFunction &fn, const std::string &displayName,
            std::size_t dataRow, const char *dataTag)
{
    state.beginFunction(plan.className, displayName.c_str());
    if (dataRow != PkTestCase::NoDataRow) {
        state.setCurrentDataRow(dataRow, dataTag);
    }
    invokeIfPresent(obj, plan.initFn);
    fn.invoke(obj);
    invokeIfPresent(obj, plan.cleanupFn);
    state.endFunction();
}

} // namespace

int execPlan(PkTestObject *obj, const PkTestPlan &plan, int argc, char **argv)
{
    const std::vector<std::string> filters = parseFilters(argc, argv);

    std::printf("********* Start testing of %s *********\n", plan.className);

    if (!validateFilters(filters, plan.functions, plan.count)) {
        std::printf("********* Finished testing of %s *********\n", plan.className);
        return 1;
    }

    PkTestCase &state = PkTestCase::current();
    state.beginRun();

    state.beginFunction(plan.className, "initTestCase()");
    invokeIfPresent(obj, plan.initTestCase);
    const bool initFailed = state.endFunction();
    // QSKIP 在 initTestCase 里跳过的是**整个测试类**，不只是 initTestCase 自己。
    const bool initSkipped = state.lastFunctionSkipped();

    if (!initFailed && !initSkipped) {
        for (int i = 0; i < plan.count; ++i) {
            const PkTestFunction &fn = plan.functions[i];
            if (!selected(filters, fn.name)) {
                continue;
            }

            if (!fn.dataName) {
                runOnce(obj, plan, state, fn,
                        std::string(fn.name) + "()", PkTestCase::NoDataRow, nullptr);
                continue;
            }

            // 数据驱动：先建表，表为空则这个测试函数一次都不跑
            // （QTest 语义——漏了这条会让空表静默退化成一次无参调用）。
            //
            // 建表调用必须包在 beginFunction/endFunction 之间：_data() 函数体里
            // 一样能写断言（真实调用点：libs/ui/tests/KisMultiFeedRssModelTest.cpp
            // 的 testAddFeed_data），落在状态机之外的话失败会被记进上一个函数的
            // 残留上下文、再被下一次 beginFunction 抹掉，Totals 报 0 failed。
            PkTestTable::current().clear();
            const PkTestFunction *dataFn =
                findByName(plan.dataFunctions, plan.dataCount, fn.dataName);
            state.beginFunction(plan.className, (std::string(fn.dataName) + "()").c_str());
            invokeIfPresent(obj, dataFn);
            const bool dataFailed = state.endFunction();
            if (dataFailed) {
                // 建表就挂了，逐行跑没有意义 —— 每一行都会在同一个坏表上再挂一次。
                continue;
            }

            const PkTestTable &table = PkTestTable::current();
            const std::size_t rowCount = table.rowCount();
            for (std::size_t r = 0; r < rowCount; ++r) {
                const std::string tag = table.tagAt(r);
                runOnce(obj, plan, state, fn, fn.name + ("(" + tag + ")"), r, tag.c_str());
            }
        }
    }

    state.beginFunction(plan.className, "cleanupTestCase()");
    invokeIfPresent(obj, plan.cleanupTestCase);
    state.endFunction();

    // skipped 必须打出来：全部被跳过的一次运行与真正全绿的 rc 都是 0，
    // 不打这个数就在输出上无法区分。Qt 打的是 passed/failed/skipped/blacklisted，
    // 我们没有 blacklist 机制，只打前三个。
    std::printf("Totals: %d passed, %d failed, %d skipped\n",
                state.passedFunctionCount(), state.failedFunctionCount(),
                state.skippedFunctionCount());
    std::printf("********* Finished testing of %s *********\n", plan.className);

    return state.failedFunctionCount();
}

// Qt 里 QTest::qFail 是 QFAIL/QVERIFY2 等宏内部实际调用的记录函数，返回值恒为
// false（调用方靠它省一次分支）。这里直接复用 recordFailure——它就是
// checkResult() 失败分支背后那同一条记录路径，PK_FAIL 也走它。
bool qFail(const char *message, const char *file, int line)
{
    PkTestCase::current().recordFailure(file, line, message ? message : "");
    return false;
}

} // namespace PkTest
