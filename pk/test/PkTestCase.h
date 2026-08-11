#pragma once
#include <string>

// XFAIL 之后的执行策略：QTest::Continue 继续执行函数体，QTest::Abort 立即 return。
enum PkTestFailMode { PkTestContinueMode, PkTestAbortMode };

// 一次 qExec 期间的运行状态。单例而非参数传递，因为断言宏在测试函数体里展开，
// 拿不到任何上下文对象 —— QTest 用的也是同一套思路（QTestResult 的静态状态）。
class PkTestCase
{
public:
    static PkTestCase &current();

    // 每次 qExec 开跑前调用一次：把通过/失败/跳过计数清零。
    //
    // PkTestCase 是进程级单例（断言宏拿不到别的上下文），但类头注释写的是
    // "一次 qExec 期间的运行状态"——计数必须按 run 归零，不能跨 qExec 调用累加。
    // 一个测试二进制通常只 qExec 一次，这条不显眼；本 harness 的自测里一个
    // 进程要连续 qExec 好几个测试类，会立刻暴露不归零的计数错误。
    void beginRun();

    void beginFunction(const char *className, const char *functionName);
    // 返回该函数是否失败
    bool endFunction();

    void recordFailure(const char *file, int line, const std::string &message);

    // 断言结果的统一入口。返回 true 表示"调用方应当 return"。
    // ok=true 且有 expect-fail 标记 → XPASS（计失败，返回 true）
    // ok=false 且有 expect-fail 标记 → XFAIL（不计失败，Abort 模式返回 true）
    // ok=false 且无标记 → 真失败（返回 true）
    // ok=true 且无标记 → 通过（返回 false）
    bool checkResult(bool ok, const char *file, int line, const std::string &message);

    // 给紧接着的下一个断言打 expect-fail 标记，用完即清。
    // dataIndex 为空对所有数据行生效；非空则只在当前数据行 tag 与它相符时才 arm。
    void expectFail(const char *dataIndex, const char *comment, PkTestFailMode mode,
                    const char *file, int line);

    void skipCurrent(const char *message, const char *file, int line);

    // 当前数据行的 tag，供 expectFail 的 dataIndex 匹配、也供 PkTestData 按 tag 找行。
    void setCurrentDataTag(const char *tag);
    const std::string &currentDataTag() const { return m_currentDataTag; }

    int failedFunctionCount() const { return m_failedFunctions; }
    int passedFunctionCount() const { return m_passedFunctions; }
    int skippedFunctionCount() const { return m_skippedFunctions; }

private:
    PkTestCase() = default;

    std::string m_className;
    std::string m_functionName;
    bool m_currentFailed = false;
    int m_failedFunctions = 0;
    int m_passedFunctions = 0;

    bool m_expectFailArmed = false;
    std::string m_expectFailComment;
    PkTestFailMode m_expectFailMode = PkTestContinueMode;

    std::string m_currentDataTag;
    bool m_currentSkipped = false;
    int m_skippedFunctions = 0;
};
