// ============================================================================
// 试接垫片的**实现侧** —— 不是 R-03 的交付物。
//
// 这里只放"头文件是真的、缺的是定义"的那几个符号。它们的头文件都用真品：
//   · libs/global/kis_assert.h（真品，本目录**没有**同名垫片）声明了四个
//     KRITAGLOBAL_EXPORT 函数，实现在 libs/global/kis_assert.cpp，而那个 .cpp
//     依赖 QMessageBox / KisAssertException / QThread 一整套 UI 与异常设施
//     （归 R-08 与 S 线），试接期链不进来 —— 所以定义在这里补。
//     **头是真的这一点很重要**：KIS_ASSERT 家族的宏体、参数顺序、
//     "recoverable 返回后继续执行"这条语义全部来自真品，没有被垫片改写。
//   · KisUsageLogger::log —— 见 stubs/KisUsageLogger.h 的头注释。
//
// 行为选择：
//   · *_recoverable 两个：打到 stderr 然后**返回**（真品也是"记一笔、继续跑"）。
//     试接期真触发了会在输出里看见，不会被静默吞掉。
//   · kis_assert_exception / kis_assert_x_exception：真品**抛** KisAssertException
//     让事件循环重启。试接没有事件循环也没有异常体系，这里 abort()。
//     选 abort 而不是"打一行然后继续"是有意的：继续跑会让一个断言失败的测试
//     照样报 PASS，那比直接死掉危险得多。
// ============================================================================
#include <cstdio>
#include <cstdlib>

#include <kis_assert.h>

#include "KisUsageLogger.h"

void kis_assert_exception(const char *assertion, const char *file, int line)
{
    std::fprintf(stderr, "[graft] KIS_ASSERT failed: %s (%s:%d)\n", assertion, file, line);
    std::abort();
}

void kis_assert_x_exception(const char *assertion, const char *where,
                            const char *what, const char *file, int line)
{
    std::fprintf(stderr, "[graft] KIS_ASSERT_X failed: %s in %s: %s (%s:%d)\n",
                 assertion, where, what, file, line);
    std::abort();
}

void kis_assert_recoverable(const char *assertion, const char *file, int line)
{
    std::fprintf(stderr, "[graft] KIS_ASSERT_RECOVER: %s (%s:%d)\n", assertion, file, line);
}

void kis_safe_assert_recoverable(const char *assertion, const char *file, int line)
{
    std::fprintf(stderr, "[graft] KIS_SAFE_ASSERT_RECOVER: %s (%s:%d)\n", assertion, file, line);
}

// 真品把消息写进 usage log 文件。试接期丢掉 —— 唯一调用点在一条走不到的
// 防御分支里，把它打出来只会污染 harness 的判定行。
void KisUsageLogger::log(const QString &)
{
}
