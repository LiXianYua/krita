// ===========================================================================
// R-53 Task 1 —— 跨镜像 any_cast 回归用例（exe 镜像一侧）
// ===========================================================================
//
// 本 TU 编进测试 exe test_pk_cross_image。这个 exe 同时链接：
//   · libpkvariant.a 等 STATIC 库 —— exe 镜像自己的那份 pk 目标码
//   · pkcrossimage（SHARED）    —— 里面另静态链了一份 pk 目标码，即库镜像
// ⇒ 进程里同一条 any_cast 判等路径存在**两份实体**，用各自的 type_info 副本。
// 这正是主树里 `libkritaglobal.dylib ↔ 测试 exe` 那条边界的薄壳复刻
// （薄壳原本全是 STATIC + 单 exe，边界不存在；plan §3 第 2 问）。
//
// PK_* harness 形制照 tests/test_variant.cpp：
//   #include "pk_binder_<case>.inc" + PK_TEST_MAIN(<Case>)
// 断言宏失败会从当前测试函数 return（QTest 语义），所以下面每个槽名就是
// ctest 报出来的**用例名**。

#include "cross_image_case.h"

#include "cross_image_payload.h"

#include "pk_binder_cross_image_case.inc"

#include <cstdio>
#include <cstdint>
#include <string>

// ── 每个类型的用例体 ──────────────────────────────────────────────────────
// 三件事，缺一不可：
//   ① 本地健全性：exe 镜像造 + exe 镜像读 —— 恒须绿。它证明「载体本身没坏」，
//      于是 ②③ 的红只可能来自跨镜像那一步（brief 完成条件 2 的对照）。
//   ② 方向 A：exe 造 / lib 读
//   ③ 方向 B：lib 造 / exe 读
// ②③ 在**没挂属性**的树上必须红；S-17 已修的 3 个类型则是 ②③ 恒绿的对照组。
//
// ②③ **必须先各自求值、再断言**：PK_VERIFY* 一失败就从用例 return（QTest 语义），
// 写成两条顺序断言的话，前一条红了后面那条根本不会跑，两个方向就只测到一个。
// 这里两条都跑完，再把「哪个方向读不到」写进失败消息。
//
// T 只出现在 ximg_* 前缀的标识符里——槽名恰好就是类型名，类作用域里裸写 T
// 会先命中成员函数而不是类型，这里刻意避开。
#define XIMG_CASE_BODY(T)                                                     \
    do {                                                                      \
        const PkVariant exeMade = ximg_make_##T();                            \
        PK_VERIFY(exeMade.type() != PkVariant::Invalid);                      \
        PK_VERIFY(ximg_read_##T(exeMade));                                    \
        const PkVariant libMade = ximg_lib_make_##T();                        \
        PK_VERIFY(libMade.type() != PkVariant::Invalid);                      \
        const bool dirA = ximg_lib_read_##T(exeMade);  /* exe 造 / lib 读 */  \
        const bool dirB = ximg_read_##T(libMade);      /* lib 造 / exe 读 */  \
        const std::string why =                                               \
            std::string(dirA ? "" : "方向A(exe造/lib读) 跨镜像读不到; ")      \
            + (dirB ? "" : "方向B(lib造/exe读) 跨镜像读不到");                \
        PK_VERIFY2(dirA && dirB, why);                                        \
    } while (false)

#define XIMG_DEFINE_SLOT(T, V)                                                \
    void CrossImageCase::T()                                                  \
    {                                                                         \
        if (!ximgPlatformSupported()) {                                       \
            return;                                                           \
        }                                                                     \
        XIMG_CASE_BODY(T);                                                    \
    }
XIMG_PAYLOAD_TYPES(XIMG_DEFINE_SLOT)
#undef XIMG_DEFINE_SLOT
#undef XIMG_CASE_BODY

// ── 证据槽：unique 位的运行期实测（plan §1.3 的方法 / §5 Task 1 的验收项）──
// 只打印、不断言，恒过——红段与绿段都要有这一份读数，红证据里引用的就是它。
// 口径：**不读 nm 的分类**（实测 nm -m 对加不加 -fvisibility=hidden 输出逐字相同，
// 没有判别力），只读运行期 type_info.__type_name 的最高位。
void CrossImageCase::typeInfoUniqueBit()
{
    std::printf("R-53 Task 1 —— libc++ type_info unique 位实测（运行期读数）\n");
    std::printf("口径: type_info 布局 [vptr][__type_name]，读 ((uintptr_t*)&typeid(T))[1]；\n");
    std::printf("      最高位 1 = non-unique（属性生效，any_cast 走 strcmp 回退）；0 = unique（只比地址）\n");
    std::printf("%-14s %-20s %-6s %-20s %-6s\n", "type", "exe __type_name", "exe", "lib __type_name", "lib");

#define XIMG_DUMP_ROW(T, V)                                                   \
    do {                                                                      \
        const std::uintptr_t exeRaw = ximg_rawname_##T();                     \
        const std::uintptr_t libRaw = ximg_lib_rawname_##T();                 \
        std::printf("%-14s 0x%016llx %-6d 0x%016llx %-6d\n", #T,              \
                    static_cast<unsigned long long>(exeRaw),                  \
                    static_cast<int>(ximgIsNonUnique(exeRaw)),                \
                    static_cast<unsigned long long>(libRaw),                  \
                    static_cast<int>(ximgIsNonUnique(libRaw)));               \
    } while (false);
    XIMG_PAYLOAD_TYPES(XIMG_DUMP_ROW)
#undef XIMG_DUMP_ROW

    std::fflush(stdout);
}

PK_TEST_MAIN(CrossImageCase)
