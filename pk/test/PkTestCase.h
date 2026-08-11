#pragma once
#include <string>

// 一次 qExec 期间的运行状态。单例而非参数传递，因为断言宏在测试函数体里展开，
// 拿不到任何上下文对象 —— QTest 用的也是同一套思路（QTestResult 的静态状态）。
class PkTestCase
{
public:
    static PkTestCase &current();

    void beginFunction(const char *className, const char *functionName);
    // 返回该函数是否失败
    bool endFunction();

    void recordFailure(const char *file, int line, const std::string &message);

    int failedFunctionCount() const { return m_failedFunctions; }
    int passedFunctionCount() const { return m_passedFunctions; }

private:
    PkTestCase() = default;

    std::string m_className;
    std::string m_functionName;
    bool m_currentFailed = false;
    int m_failedFunctions = 0;
    int m_passedFunctions = 0;
};
