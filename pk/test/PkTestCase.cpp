#include "PkTestCase.h"
#include <cstdio>

PkTestCase &PkTestCase::current()
{
    static PkTestCase instance;
    return instance;
}

void PkTestCase::beginFunction(const char *className, const char *functionName)
{
    m_className = className ? className : "";
    m_functionName = functionName ? functionName : "";
    m_currentFailed = false;
}

bool PkTestCase::endFunction()
{
    if (m_currentFailed) {
        ++m_failedFunctions;
        std::printf("FAIL!  : %s::%s()\n", m_className.c_str(), m_functionName.c_str());
    } else {
        ++m_passedFunctions;
        std::printf("PASS   : %s::%s()\n", m_className.c_str(), m_functionName.c_str());
    }
    return m_currentFailed;
}

void PkTestCase::recordFailure(const char *file, int line, const std::string &message)
{
    m_currentFailed = true;
    std::printf("FAIL!  : %s::%s() %s\n   Loc: [%s(%d)]\n",
                m_className.c_str(), m_functionName.c_str(),
                message.c_str(), file, line);
}
