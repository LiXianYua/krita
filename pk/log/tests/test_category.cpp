#include "test_category.h"
#include "../PkLogSink.h"
#include "../PkLoggingCategory.h"
#include "../PkMessageLogger.h"
#include <string>
#include <vector>

// ① 三参形态的默认级别生效：QtInfoMsg 的分类，debug 不启用、info 启用。
void PkLoggingCategoryTest::testThreeArgFormSetsMinimumLevel()
{
    PkLoggingCategory cat("krita.file", PkLogInfo);
    PK_VERIFY(!cat.isDebugEnabled());
    PK_VERIFY(cat.isInfoEnabled());
    PK_VERIFY(cat.isWarningEnabled());
    PK_VERIFY(cat.isCriticalEnabled());
}

// ②③ 惰性求值：分类禁用时 qCDebug(cat) << expensive() 里的 expensive() 一次
// 都不许求值；启用时恰好求值一次。两个测试分类分别落在两侧：一个 QtInfoMsg
// （debug 禁用），一个 QtDebugMsg（debug 启用，即两参默认级别）。
namespace {

int g_evalCount = 0;
int expensive()
{
    ++g_evalCount;
    return 1;
}

} // namespace

PK_LOGGING_CATEGORY(pkLogTestCatDisabled, "test.category.disabled", PkLogInfo)
PK_LOGGING_CATEGORY(pkLogTestCatEnabled, "test.category.enabled", PkLogDebug)

// 展开成 qCDebug(pkLogTestCatDisabled) << expensive();——与 brief 里
// "qCDebug(cat) << expensive();" 同形，cat 换成本文件的测试分类函数名。
#define PK_LOG_TEST_CATEGORY_DISABLED_DEBUG() qCDebug(pkLogTestCatDisabled) << expensive()
#define PK_LOG_TEST_CATEGORY_ENABLED_DEBUG() qCDebug(pkLogTestCatEnabled) << expensive()

void PkLoggingCategoryTest::testDisabledCategoryDoesNotEvaluateArguments()
{
    g_evalCount = 0;
    PK_LOG_TEST_CATEGORY_DISABLED_DEBUG();
    PK_COMPARE(g_evalCount, 0);
}

void PkLoggingCategoryTest::testEnabledCategoryEvaluatesArgumentsOnce()
{
    g_evalCount = 0;
    PK_LOG_TEST_CATEGORY_ENABLED_DEBUG();
    PK_COMPARE(g_evalCount, 1);
}

// ④ 分类名进到 sink 的 context 里。
namespace {

std::vector<std::string> g_categories;
void captureCategory(PkLogLevel, const PkLogContext &ctx, const char *, void *)
{
    g_categories.push_back(ctx.category);
}

} // namespace

void PkLoggingCategoryTest::testCategoryNameReachesSink()
{
    g_categories.clear();
    const int h = PkLogAddSink(captureCategory, nullptr);
    qCWarning(pkLogTestCatEnabled) << "hello";
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_categories.size()), 1);
    PK_COMPARE(g_categories[0], std::string("test.category.enabled"));
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_category.inc"

int run_category_tests(int argc, char **argv)
{
    PkLoggingCategoryTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
