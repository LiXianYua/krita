#include "PkTest.h"
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

} // namespace

int execPlan(PkTestObject *obj, const PkTestPlan &plan, int argc, char **argv)
{
    const std::vector<std::string> filters = parseFilters(argc, argv);

    std::printf("********* Start testing of %s *********\n", plan.className);

    PkTestCase &state = PkTestCase::current();

    state.beginFunction(plan.className, "initTestCase");
    invokeIfPresent(obj, plan.initTestCase);
    const bool initFailed = state.endFunction();

    if (!initFailed) {
        for (int i = 0; i < plan.count; ++i) {
            const PkTestFunction &fn = plan.functions[i];
            if (!selected(filters, fn.name)) {
                continue;
            }
            state.beginFunction(plan.className, fn.name);
            invokeIfPresent(obj, plan.initFn);
            fn.invoke(obj);
            invokeIfPresent(obj, plan.cleanupFn);
            state.endFunction();
        }
    }

    state.beginFunction(plan.className, "cleanupTestCase");
    invokeIfPresent(obj, plan.cleanupTestCase);
    state.endFunction();

    std::printf("Totals: %d passed, %d failed\n",
                state.passedFunctionCount(), state.failedFunctionCount());
    std::printf("********* Finished testing of %s *********\n", plan.className);

    return state.failedFunctionCount();
}

} // namespace PkTest
