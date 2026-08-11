#include "../PkTest.h"
#include "../PkTestData.h"
#include "selftest_util.h"
#include <cstring>
#include <string>
#include <vector>

static std::vector<std::string> g_seenTags;
static int g_sumOfInputs = 0;
static int g_emptyTableRuns = 0;
static bool g_typeMismatchReachedAfterFetch = false;
static double g_typeMismatchFetchedValue = -1.0;   // 哨兵值：应变成 double() == 0.0

class SelfDataCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    void adds_data()
    {
        PkTest::addColumn<int>("lhs");
        PkTest::addColumn<int>("rhs");
        PkTest::addColumn<int>("sum");
        PkTest::newRow("zero")     << 0 << 0 << 0;
        PkTest::newRow("positive") << 2 << 3 << 5;
        PkTest::addRow("gen-%d", 7) << 7 << 1 << 8;
    }
    void adds()
    {
        PK_FETCH(int, lhs);
        PK_FETCH(int, rhs);
        PK_FETCH(int, sum);
        g_seenTags.push_back(PkTest::currentDataTag());
        g_sumOfInputs += lhs + rhs;
        PK_COMPARE(lhs + rhs, sum);
    }
    void emptyTable_data() { PkTest::addColumn<int>("unused"); }
    void emptyTable()      { ++g_emptyTableRuns; }

    // dataIndex 非空：只对 tag 相符的那一行 arm。"good" 行不 arm、断言真通过；
    // "bad" 行 arm、断言真失败但被豁免成 XFAIL —— 两行都不计入失败。
    void partialExpectFail_data()
    {
        PkTest::addColumn<int>("v");
        PkTest::newRow("good") << 1;   // 不 arm：v==1 与期望值相符，真通过
        PkTest::newRow("bad")  << 2;   // arm：v==2 与期望值不符，真失败 → XFAIL
    }
    void partialExpectFail()
    {
        PK_EXPECT_FAIL("bad", "只有这一行已知会挂", Continue);
        PK_FETCH(int, v);
        PK_COMPARE(v, 1);
    }
};

template <> struct PkTestBinder<SelfDataCase> {
    static const char *className() { return "SelfDataCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"adds",              [](PkTestObject *o){ static_cast<SelfDataCase*>(o)->adds(); },              "adds_data"},
            {"emptyTable",        [](PkTestObject *o){ static_cast<SelfDataCase*>(o)->emptyTable(); },        "emptyTable_data"},
            {"partialExpectFail", [](PkTestObject *o){ static_cast<SelfDataCase*>(o)->partialExpectFail(); }, "partialExpectFail_data"},
        };
        return fns;
    }
    static int count() { return 3; }
    // 数据函数单独一张表，runner 按 dataName 查
    static const PkTestFunction *dataFunctions() {
        static const PkTestFunction fns[] = {
            {"adds_data",              [](PkTestObject *o){ static_cast<SelfDataCase*>(o)->adds_data(); },              nullptr},
            {"emptyTable_data",        [](PkTestObject *o){ static_cast<SelfDataCase*>(o)->emptyTable_data(); },        nullptr},
            {"partialExpectFail_data", [](PkTestObject *o){ static_cast<SelfDataCase*>(o)->partialExpectFail_data(); }, nullptr},
        };
        return fns;
    }
    static int dataCount() { return 3; }
    static const PkTestFunction *initTestCase()     { return nullptr; }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

// PK_FETCH 的 Type 与 addColumn<T> 登记的类型不一致：应记一次失败而不是 UB/崩溃
// ——Qt 里 QFETCH 类型写错是运行时报错，不是未定义行为，这条要有自测钉住。
class SelfDataTypeMismatchCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    void mismatch_data()
    {
        PkTest::addColumn<int>("v");   // 列登记的类型是 int
        PkTest::newRow("row1") << 7;
    }
    void mismatch()
    {
        PK_FETCH(double, v);           // 取值时要的类型是 double —— 类型不符
        g_typeMismatchFetchedValue = v;
        g_typeMismatchReachedAfterFetch = true;   // 能跑到这里，证明没崩
    }
};

template <> struct PkTestBinder<SelfDataTypeMismatchCase> {
    static const char *className() { return "SelfDataTypeMismatchCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"mismatch", [](PkTestObject *o){ static_cast<SelfDataTypeMismatchCase*>(o)->mismatch(); }, "mismatch_data"},
        };
        return fns;
    }
    static int count() { return 1; }
    static const PkTestFunction *dataFunctions() {
        static const PkTestFunction fns[] = {
            {"mismatch_data", [](PkTestObject *o){ static_cast<SelfDataTypeMismatchCase*>(o)->mismatch_data(); }, nullptr},
        };
        return fns;
    }
    static int dataCount() { return 1; }
    static const PkTestFunction *initTestCase()     { return nullptr; }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

// 重复 tag：两行同名，取值必须按行号而不是按 tag 反查
// （真实调用点：libs/image/tests/TestAslStorage.cpp 的 testResource_data）。
static std::vector<int> g_dupTagValues;

class SelfDupTagCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    void dup_data()
    {
        PkTest::addColumn<int>("v");
        PkTest::newRow("dup") << 1;
        PkTest::newRow("dup") << 2;
    }
    void dup()
    {
        PK_FETCH(int, v);
        g_dupTagValues.push_back(v);
    }
};

template <> struct PkTestBinder<SelfDupTagCase> {
    static const char *className() { return "SelfDupTagCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"dup", [](PkTestObject *o){ static_cast<SelfDupTagCase*>(o)->dup(); }, "dup_data"},
        };
        return fns;
    }
    static int count() { return 1; }
    static const PkTestFunction *dataFunctions() {
        static const PkTestFunction fns[] = {
            {"dup_data", [](PkTestObject *o){ static_cast<SelfDupTagCase*>(o)->dup_data(); }, nullptr},
        };
        return fns;
    }
    static int dataCount() { return 1; }
    static const PkTestFunction *initTestCase()     { return nullptr; }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

// _data() 函数体里的断言失败必须被归属与计数，且该测试函数不再逐行执行
// （真实调用点：libs/ui/tests/KisMultiFeedRssModelTest.cpp 的 testAddFeed_data）。
static int g_afterFailedDataRuns = 0;
// 值的类型与 addColumn 登记的列类型不符：入表阶段就该点名（与 Qt 一致）。
static int g_afterWrongColumnTypeRuns = 0;

class SelfBrokenDataCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    void failingBuild_data()
    {
        PkTest::addColumn<int>("v");
        PkTest::newRow("row") << 1;
        PK_VERIFY(false);          // 建表阶段就挂
    }
    void failingBuild() { ++g_afterFailedDataRuns; }

    void wrongColumnType_data()
    {
        PkTest::addColumn<int>("v");
        PkTest::newRow("row") << 2.5;   // double 值进 int 列
    }
    void wrongColumnType() { ++g_afterWrongColumnTypeRuns; }
};

template <> struct PkTestBinder<SelfBrokenDataCase> {
    static const char *className() { return "SelfBrokenDataCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"failingBuild",    [](PkTestObject *o){ static_cast<SelfBrokenDataCase*>(o)->failingBuild(); },    "failingBuild_data"},
            {"wrongColumnType", [](PkTestObject *o){ static_cast<SelfBrokenDataCase*>(o)->wrongColumnType(); }, "wrongColumnType_data"},
        };
        return fns;
    }
    static int count() { return 2; }
    static const PkTestFunction *dataFunctions() {
        static const PkTestFunction fns[] = {
            {"failingBuild_data",    [](PkTestObject *o){ static_cast<SelfBrokenDataCase*>(o)->failingBuild_data(); },    nullptr},
            {"wrongColumnType_data", [](PkTestObject *o){ static_cast<SelfBrokenDataCase*>(o)->wrongColumnType_data(); }, nullptr},
        };
        return fns;
    }
    static int dataCount() { return 2; }
    static const PkTestFunction *initTestCase()     { return nullptr; }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

// 字符串字面量喂进数据行：operator<< 里 T 由模板推导（数组字面量推出来是
// char[N]，未 decay），但 std::any(value) 内部把它 decay 成 const char*
// 再存。appendValue 与 valueAt() 必须用同一个已 decay 的类型身份比对，
// 否则这一行值在入表阶段就会被当成"类型不符"拒收
// （见 PkTestData.cpp 的 appendValue 注释）。
static std::string g_cstrFetched;

class SelfDataConstCharCase : public PkTestObject
{
    template <typename PkTestBinderArgT> friend struct PkTestBinder;
private:
    void cstr_data()
    {
        PkTest::addColumn<const char*>("s");
        PkTest::newRow("r") << "hello";
    }
    void cstr()
    {
        PK_FETCH(const char*, s);
        g_cstrFetched = s ? s : "";
    }
};

template <> struct PkTestBinder<SelfDataConstCharCase> {
    static const char *className() { return "SelfDataConstCharCase"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"cstr", [](PkTestObject *o){ static_cast<SelfDataConstCharCase*>(o)->cstr(); }, "cstr_data"},
        };
        return fns;
    }
    static int count() { return 1; }
    static const PkTestFunction *dataFunctions() {
        static const PkTestFunction fns[] = {
            {"cstr_data", [](PkTestObject *o){ static_cast<SelfDataConstCharCase*>(o)->cstr_data(); }, nullptr},
        };
        return fns;
    }
    static int dataCount() { return 1; }
    static const PkTestFunction *initTestCase()     { return nullptr; }
    static const PkTestFunction *cleanupTestCase()  { return nullptr; }
    static const PkTestFunction *initFn()           { return nullptr; }
    static const PkTestFunction *cleanupFn()        { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

void run_data_selftests()
{
    SelfDataCase tc;
    const char *argv[] = {"selftest"};
    const int rc = PkTest::qExec(&tc, 1, const_cast<char **>(argv));

    SELF_EXPECT(rc == 0,
                "数据驱动的三行都应通过；partialExpectFail 的 good/bad 两行"
                "（一行真通过、一行按 dataIndex 精确匹配被豁免成 XFAIL）也都不计入失败");
    SELF_EXPECT(g_seenTags.size() == 3, "三行数据 → 测试函数跑三次");
    // SELF_EXPECT 不中断执行，所以下标访问必须自己守住 size ——
    // 上一条断言挂掉时无条件索引 [0]..[2] 是自测自身的 UB。
    if (g_seenTags.size() == 3) {
        SELF_EXPECT(g_seenTags[0] == "zero"      , "第一行 tag");
        SELF_EXPECT(g_seenTags[1] == "positive"  , "第二行 tag");
        SELF_EXPECT(g_seenTags[2] == "gen-7"     , "addRow 的 printf 风格 tag");
    }
    SELF_EXPECT(g_sumOfInputs == 0 + 0 + 2 + 3 + 7 + 1, "每行的值都正确取到");
    SELF_EXPECT(g_emptyTableRuns == 0,
                "有 _data() 但表为空时，测试函数一次都不跑");

    SelfDataTypeMismatchCase mismatchCase;
    const int mismatchRc = PkTest::qExec(&mismatchCase, 1, const_cast<char **>(argv));
    SELF_EXPECT(mismatchRc == 1,
                "PK_FETCH 类型不符时应记一次失败（qExec 返回非 0），不是崩溃、不是 UB");
    SELF_EXPECT(g_typeMismatchReachedAfterFetch,
                "类型不符后进程没有崩溃/抛异常，函数体继续跑到了下一条语句");
    SELF_EXPECT(g_typeMismatchFetchedValue == 0.0,
                "类型不符时 PK_FETCH 退化成值初始化的默认值（double() == 0.0），不是垃圾值");

    SelfDupTagCase dupCase;
    const int dupRc = PkTest::qExec(&dupCase, 1, const_cast<char **>(argv));
    SELF_EXPECT(dupRc == 0, "重复 tag 的两行都该正常跑完");
    SELF_EXPECT(g_dupTagValues.size() == 2, "两行 → 跑两次");
    if (g_dupTagValues.size() == 2) {
        SELF_EXPECT(g_dupTagValues[0] == 1 && g_dupTagValues[1] == 2,
                    "重复 tag 时取值必须按行号：两行分别读到 1 和 2，"
                    "而不是都读到第一个 tag 相符的行");
    }

    SelfBrokenDataCase brokenCase;
    const int brokenRc = PkTest::qExec(&brokenCase, 1, const_cast<char **>(argv));
    SELF_EXPECT(brokenRc == 2,
                "_data() 里的断言失败与列类型不符各计一次失败——"
                "落在 beginFunction/endFunction 之外的话两条都会被静默吞掉，Totals 报 0 failed");
    SELF_EXPECT(g_afterFailedDataRuns == 0,
                "建表挂了之后该测试函数不再逐行执行");
    SELF_EXPECT(g_afterWrongColumnTypeRuns == 0,
                "列类型不符时该测试函数不再逐行执行");

    SelfDataConstCharCase cstrCase;
    const int cstrRc = PkTest::qExec(&cstrCase, 1, const_cast<char **>(argv));
    SELF_EXPECT(cstrRc == 0,
                "addColumn<const char*> + 字符串字面量数据行应正常入表、正常取回，"
                "不应因 appendValue/valueAt 两处类型身份不一致而被判定类型不符");
    SELF_EXPECT(g_cstrFetched == "hello", "取回的值就是喂进去的字符串字面量");
}
