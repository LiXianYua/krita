// ============================================================================
// 试接垫片的**实现侧** —— 不是 R-07 的正式交付物，是这份试接自己的构建期胶水。
// 逐条对照 pk/geometry/graft/stubs/graft_stubs.cpp 的先例处置（同类问题、同样
// 只补"头是真的、缺的是定义"这一段）：
//
//   · kis_assert.h（真品，本目录**没有**同名垫片）声明了四个 KRITAGLOBAL_EXPORT
//     函数，实现在 libs/global/kis_assert.cpp，而那个 .cpp 依赖 QMessageBox /
//     KisAssertException / QThread 一整套 UI 与异常设施（归 R-08 与 S 线），
//     试接期链不进来——定义在这里补。头是真的这一点很重要：KIS_ASSERT 家族的
//     宏体、参数顺序、"recoverable 返回后继续执行"这条语义全部来自真品，
//     没有被垫片改写。
//
// 行为选择与 pk/geometry 那份一致：
//   · *_recoverable 两个：打到 stderr 然后**返回**（真品也是"记一笔、继续跑"）。
//   · kis_assert_exception / kis_assert_x_exception：真品**抛**
//     KisAssertException 让事件循环重启。试接没有事件循环也没有异常体系，
//     这里 abort()——继续跑会让一个断言失败的测试照样报 PASS，比直接死掉危险。
//
// 本试接实测唯一真正被编译进来的是 kis_safe_assert_recoverable（kis_dom_utils.cpp
// 的 removeElements() 用 KIS_SAFE_ASSERT_RECOVER_NOOP），且从未在任何测试方法
// 里被调用到（removeElements 本身不在 kis_dom_utils_test.cpp 的调用面里）——
// 四个都补齐只是为了不给 kis_assert.h 的其余三个符号留链接期缺口，不代表
// 认领了它们的正式行为。
// ============================================================================
#include <cstdio>
#include <cstdlib>

#include <kis_assert.h>

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

// ----------------------------------------------------------------------------
// QString（本试接的 PkDomUtilsGraftString 垫片）::number(int/double) 的实现。
// 见 stubs/QString 类头注释：临时局部代打 PkString（R-01）用量表之外的一个
// 方法，唯一真实调用点是 KisDomUtils 泛型模板 `toString<T>` 对 int 值走的
// `QString::number(value)`（i1/i2/i3 三个 int 的往返，本试接的真实断言覆盖）。
// ----------------------------------------------------------------------------
#include "QString"
#include <cstdio>
#include <string>

PkDomUtilsGraftString PkDomUtilsGraftString::number(int v)
{
    return PkDomUtilsGraftString(std::to_string(v).c_str());
}

PkDomUtilsGraftString PkDomUtilsGraftString::number(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return PkDomUtilsGraftString(buf);
}

// 见 stubs/QString 类头注释：KisPortingUtils::stringRemoveLast()/
// stringRemoveFirst() 用到，KisDomUtils 的任何往返路径都不触达。UTF-8 字节
// 粒度近似（不是 Qt 的 UTF-16 码元粒度），因为没有任何断言验证这个方法的
// 精确语义。
PkDomUtilsGraftString &PkDomUtilsGraftString::remove(int pos, int n)
{
    std::string bytes = PkToUtf8();
    if (pos >= 0 && pos < static_cast<int>(bytes.size())) {
        bytes.erase(static_cast<std::size_t>(pos), static_cast<std::size_t>(n));
    }
    *this = PkDomUtilsGraftString(bytes.c_str());
    return *this;
}
