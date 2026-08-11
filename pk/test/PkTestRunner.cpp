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

// QTest 的命令行：裸参数是要跑的测试函数名（可带 :dataTag）。
// R-11 只实现"按函数名过滤"这一条 —— 它是 ctest 与人工调试都会用到的，
// 其余开关（-o/-xml/-maxwarnings/...）Krita 的测试构建没有依赖，不实现。
std::vector<std::string> parseFilters(int argc, char **argv)
{
    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && argv[i][0] != '-') {
            filters.emplace_back(argv[i]);
        }
    }
    return filters;
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
// dataTag 为 nullptr 表示非数据驱动 —— beginFunction 已经把上一次的 tag 清掉，
// 不用再显式设置一次空串。
void runOnce(PkTestObject *obj, const PkTestPlan &plan, PkTestCase &state,
            const PkTestFunction &fn, const std::string &displayName, const char *dataTag)
{
    state.beginFunction(plan.className, displayName.c_str());
    if (dataTag) {
        state.setCurrentDataTag(dataTag);
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

    PkTestCase &state = PkTestCase::current();
    state.beginRun();

    state.beginFunction(plan.className, "initTestCase()");
    invokeIfPresent(obj, plan.initTestCase);
    const bool initFailed = state.endFunction();

    if (!initFailed) {
        for (int i = 0; i < plan.count; ++i) {
            const PkTestFunction &fn = plan.functions[i];
            if (!selected(filters, fn.name)) {
                continue;
            }

            if (!fn.dataName) {
                runOnce(obj, plan, state, fn, std::string(fn.name) + "()", nullptr);
                continue;
            }

            // 数据驱动：先建表，表为空则这个测试函数一次都不跑
            // （QTest 语义——漏了这条会让空表静默退化成一次无参调用）。
            PkTestTable::current().clear();
            const PkTestFunction *dataFn =
                findByName(plan.dataFunctions, plan.dataCount, fn.dataName);
            invokeIfPresent(obj, dataFn);

            const PkTestTable &table = PkTestTable::current();
            const std::size_t rowCount = table.rowCount();
            for (std::size_t r = 0; r < rowCount; ++r) {
                const std::string tag = table.tagAt(r);
                runOnce(obj, plan, state, fn, fn.name + ("(" + tag + ")"), tag.c_str());
            }
        }
    }

    state.beginFunction(plan.className, "cleanupTestCase()");
    invokeIfPresent(obj, plan.cleanupTestCase);
    state.endFunction();

    std::printf("Totals: %d passed, %d failed\n",
                state.passedFunctionCount(), state.failedFunctionCount());
    std::printf("********* Finished testing of %s *********\n", plan.className);

    return state.failedFunctionCount();
}

} // namespace PkTest
