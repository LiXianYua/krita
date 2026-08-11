#include "../PkTest.h"
#include "../PkTestCompare.h"
#include "selftest_util.h"
#include <cmath>
#include <limits>

// 局部类不能有 static 数据成员，所以 Case 与它的标记都放到文件作用域。
static bool g_compareReached = false;

struct Case : public PkTestObject {
    void run() { PK_COMPARE(1, 2); g_compareReached = true; }
};

void run_compare_selftests()
{
    // ---- 整数与指针：严格相等 ----
    SELF_EXPECT(pkTestCompare(3, 3),   "整数相等");
    SELF_EXPECT(!pkTestCompare(3, 4),  "整数不等");
    SELF_EXPECT(pkTestCompare('a', 'a'), "字符相等");

    // ---- double：Qt 的模糊比较，不是 == ----
    SELF_EXPECT(pkTestCompare(0.1 + 0.2, 0.3),
                "0.1+0.2 与 0.3 在 Qt 语义下相等（qFuzzyCompare）");
    SELF_EXPECT(!pkTestCompare(1.0, 1.1), "明显不同的 double 不等");

    // ---- 零与次正规数：只看第一参数的分类，且不对称 ----
    SELF_EXPECT(pkTestCompare(0.0, 0.0), "零等于零");
    SELF_EXPECT(pkTestCompare(0.0, 1e-13), "t1 是零 → 看 qFuzzyIsNull(t2)，1e-13 判为零");
    SELF_EXPECT(!pkTestCompare(0.0, 1e-6), "t1 是零 → 1e-6 不是零");
    SELF_EXPECT(!pkTestCompare(1e-6, 0.0),
                "反向：t1 是正常数 → 走 qFuzzyCompare，与 0 比对必不等（qMin 为 0）");

    // ---- inf / NaN ----
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    SELF_EXPECT(pkTestCompare(inf, inf),   "+inf 等于 +inf");
    SELF_EXPECT(!pkTestCompare(inf, -inf), "+inf 不等于 -inf");
    SELF_EXPECT(pkTestCompare(nan, nan),   "Qt 语义下 NaN 等于 NaN（与 == 相反）");
    SELF_EXPECT(!pkTestCompare(nan, 1.0),  "NaN 不等于普通数");
    SELF_EXPECT(!pkTestCompare(inf, 1.0),  "inf 不等于普通数");

    // ---- float 用更宽的 epsilon ----
    SELF_EXPECT(pkFuzzyCompare(1.0f, 1.0f + 1e-7f), "float 的 1e-5 相对容差");
    SELF_EXPECT(!pkFuzzyCompare(1.0, 1.0 + 1e-7),   "double 的容差比 float 严得多");

    // ---- 混合类型：float 与 double 字面量 ----
    SELF_EXPECT(pkTestCompare(42.0f, 42.0f), "float 相等");

    // ---- 诊断字符串化 ----
    SELF_EXPECT(pkTestToString(42) == "42", "整数字符串化");
    SELF_EXPECT(pkTestToString(true) == "true", "bool 字符串化");
    SELF_EXPECT(pkTestToString(false) == "false", "bool 字符串化");
    struct Opaque { int x; };
    SELF_EXPECT(pkTestToString(Opaque{1}) == "<unprintable>",
                "无法字符串化的类型必须退化成 <unprintable>，不能编译失败");

    // ---- PK_COMPARE 失败后必须 return ----
    // 直接调用，不经 qExec —— 这里只验 return 语义
    Case c;
    g_compareReached = false;
    PkTestCase::current().beginFunction("Case", "run");
    c.run();
    PkTestCase::current().endFunction();
    SELF_EXPECT(!g_compareReached, "PK_COMPARE 失败后必须 return");
}
