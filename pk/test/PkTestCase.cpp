#include "PkTestCase.h"
#include <cstdio>

PkTestCase &PkTestCase::current()
{
    static PkTestCase instance;
    return instance;
}

void PkTestCase::beginRun()
{
    m_failedFunctions = 0;
    m_passedFunctions = 0;
    m_skippedFunctions = 0;
}

void PkTestCase::beginFunction(const char *className, const char *functionName)
{
    m_className = className ? className : "";
    m_functionName = functionName ? functionName : "";
    m_currentFailed = false;
    m_currentSkipped = false;
    m_expectFailArmed = false;
    m_currentDataTag.clear();
}

bool PkTestCase::endFunction()
{
    if (m_currentSkipped) {
        ++m_skippedFunctions;
        return false;
    }
    if (m_currentFailed) {
        ++m_failedFunctions;
        std::printf("FAIL!  : %s::%s\n", m_className.c_str(), m_functionName.c_str());
    } else {
        ++m_passedFunctions;
        std::printf("PASS   : %s::%s\n", m_className.c_str(), m_functionName.c_str());
    }
    return m_currentFailed;
}

void PkTestCase::recordFailure(const char *file, int line, const std::string &message)
{
    m_currentFailed = true;
    std::printf("FAIL!  : %s::%s %s\n   Loc: [%s(%d)]\n",
                m_className.c_str(), m_functionName.c_str(),
                message.c_str(), file, line);
}

bool PkTestCase::checkResult(bool ok, const char *file, int line, const std::string &message)
{
    if (m_expectFailArmed) {
        const bool wasContinueMode = (m_expectFailMode == PkTestContinueMode);
        const std::string comment = m_expectFailComment;
        // 标记只作用于紧接着的下一个断言，用完即清——不管这次是 XFAIL 还是 XPASS。
        m_expectFailArmed = false;
        m_expectFailComment.clear();

        if (ok) {
            // XPASS：期望失败却通过了，计入失败。不看 mode，总是让调用方 return——
            // 既然预期已经落空，继续跑下去的前提也不成立了。
            recordFailure(file, line,
                         "test was expected to fail (" + comment + ") but passed");
            return true;
        }
        // XFAIL：不计入失败。
        std::printf("XFAIL  : %s::%s %s\n   Loc: [%s(%d)]\n",
                    m_className.c_str(), m_functionName.c_str(), comment.c_str(), file, line);
        return !wasContinueMode;
    }

    if (!ok) {
        recordFailure(file, line, message);
        return true;
    }
    return false;
}

void PkTestCase::expectFail(const char *dataIndex, const char *comment, PkTestFailMode mode,
                            const char *file, int line)
{
    (void)file;
    (void)line;
    const std::string idx = dataIndex ? dataIndex : "";
    if (!idx.empty() && idx != m_currentDataTag) {
        // 这一行数据不是标记指定的那一行，不 arm——下一个断言按正常路径判定。
        return;
    }
    m_expectFailArmed = true;
    m_expectFailComment = comment ? comment : "";
    m_expectFailMode = mode;
}

void PkTestCase::skipCurrent(const char *message, const char *file, int line)
{
    m_currentSkipped = true;
    std::printf("SKIP   : %s::%s %s\n   Loc: [%s(%d)]\n",
                m_className.c_str(), m_functionName.c_str(),
                message ? message : "", file, line);
}

void PkTestCase::setCurrentDataTag(const char *tag)
{
    m_currentDataTag = tag ? tag : "";
}
