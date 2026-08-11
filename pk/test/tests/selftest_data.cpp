#include "../PkTest.h"
#include "../PkTestData.h"
#include "selftest_util.h"
#include <string>
#include <vector>

static std::vector<std::string> g_seenTags;
static int g_sumOfInputs = 0;
static int g_emptyTableRuns = 0;

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
        PK_EXPECT_FAIL("bad", "只有这一行已知会挂", PkTest::Continue);
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

void run_data_selftests()
{
    SelfDataCase tc;
    const char *argv[] = {"selftest"};
    const int rc = PkTest::qExec(&tc, 1, const_cast<char **>(argv));

    SELF_EXPECT(rc == 0,
                "数据驱动的三行都应通过；partialExpectFail 的 good/bad 两行"
                "（一行真通过、一行按 dataIndex 精确匹配被豁免成 XFAIL）也都不计入失败");
    SELF_EXPECT(g_seenTags.size() == 3, "三行数据 → 测试函数跑三次");
    SELF_EXPECT(g_seenTags[0] == "zero"      , "第一行 tag");
    SELF_EXPECT(g_seenTags[1] == "positive"  , "第二行 tag");
    SELF_EXPECT(g_seenTags[2] == "gen-7"     , "addRow 的 printf 风格 tag");
    SELF_EXPECT(g_sumOfInputs == 0 + 0 + 2 + 3 + 7 + 1, "每行的值都正确取到");
    SELF_EXPECT(g_emptyTableRuns == 0,
                "有 _data() 但表为空时，测试函数一次都不跑");
}
