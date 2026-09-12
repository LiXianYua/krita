/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// ============================================================================
// ⚠ 这是「复刻调用点形状的 driver」，不是测试文件。
//
// **这不是 `plugins/impex/heif/tests/KisHeifTest.cpp`，是复刻
// `HeifImport::convert`（`plugins/impex/heif/HeifImport.cpp:226`）里
// `profileFor(...)` → `profile->name()`（`:356`/`:365`，nclx 分支）调用点形状的 driver**
// ——因为真实测试类在本 worktree 里不可用（见下「依赖墙」）。
//
// 本 driver 逐行复刻的真实调用形状（`HeifImport.cpp:340–365`）：
//   - `colorants` 取空 `PkVector<double>()`：复刻 `:340-354` 在
//     `primaries == PRIMARIES_UNSPECIFIED` 时的返回（空）；`colorants` **不参与**
//     `profileFor` 前四个分支的判定（判的是枚举 `colorPrimaries`/`transferFunction`）。
//   - `KoColorSpaceRegistry::instance()->profileFor(colorants,
//                                                    PRIMARIES_ITU_R_BT_709_5,
//                                                    TRC_IEC_61966_2_1)`（`:356-358`）。
//   - `dbgFile << "nclx profile found" << profile->name();`（`:365`）——修前形态**直接
//     解引用** `profile`，无判空。
//   对照调用：`profileFor({}, PRIMARIES_ITU_R_BT_2020_2_AND_2100_0, TRC_LINEAR)`
//   （转发到 `p2020G10Profile()`），顺带把该访问器钉住（与 R-72 Task 1 交叉验证）。
//   校验值全部来自本进程对真实内核的实测打印，不猜。
//
// 依赖墙（R-72 plan §1 F5）：
//   · `KisHeifTest` 在 APPLE 上是 `krita_add_broken_unit_test`
//     （`plugins/impex/heif/tests/CMakeLists.txt:1-6`），被 ctest 的
//     `KRITA_BROKEN_TESTS` 排除，不在判定范围；
//   · 真正走到 `:365` 还需要 libheif 句柄 + 一份带 nclx
//     （primaries=BT.709、transfer=IEC_61966_2_1）的**真 `.heic` 样本文件**。
//   这堵墙什么时候会被拆掉：需要可用的 heif 导入测试（R-65 收口测试侧 target）+
//   上述样本文件。届时本 driver 可退役，改回编译/运行真实测试类。
//
// 命令行开关：
//   （无参数）            默认**无条件** `unsetenv("EXTRA_RESOURCE_DIRS")`，把进程钉在
//                         「无资源」条件（封闭，ctest 路径不随调用者环境漂移）。
//   --keep-resource-dirs  保留调用者设的 `EXTRA_RESOURCE_DIRS`（资源齐备对照组）。
//   --unguarded-shape     走**修前形态**（R-72 裁决 B 改动**之前**、`:365` 的原样）：
//                         直接 `profile->name()`、不做判空。无 `EXTRA_RESOURCE_DIRS`
//                         下 `profileFor(...)` 返空 ⇒ 期望 SIGSEGV（退出码 139）。
// ============================================================================

#include <KoColorProfile.h>
#include <KoColorProfileConstants.h>
#include <KoColorSpaceRegistry.h>
#include <LcmsEnginePlugin.h>

#include <PkString.h>
#include <PkVector.h>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

int failures = 0;

} // namespace

int main(int argc, char **argv)
{
    // 1. 解析开关。默认把进程钉死在「无资源」条件（与 R-59/R-61/本任务 Task 1 常驻载体
    //    同款；放最前）。
    bool keepResourceDirs = false;
    bool unguardedShape = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--keep-resource-dirs") == 0) {
            keepResourceDirs = true;
        } else if (std::strcmp(argv[i], "--unguarded-shape") == 0) {
            unguardedShape = true;
        }
    }
    if (!keepResourceDirs) {
        ::unsetenv("EXTRA_RESOURCE_DIRS");
    }

    // 2. 注册 lcms 引擎——elle 剖面（p709SRGBProfile()/p2020G10Profile() 的来源）由它
    //    从资源目录载入；不注册则两个访问器都取不到（读码所得、未经探针实测：本 driver
    //    与所有探针都先注册引擎，从未喂过「未注册引擎」这一条件）。
    registerLcmsEngine();

    // 3. 打印前提（实测值，不猜）。
    const char *extra = std::getenv("EXTRA_RESOURCE_DIRS");
    std::printf("PREMISE EXTRA_RESOURCE_DIRS=%s\n", extra ? extra : "(unset)");

    // 被复刻的调用点一侧：`KoColorSpaceRegistry::instance()->p709SRGBProfile()`
    // （`profileFor(..., BT.709, IEC_61966_2_1)` 在 `KoColorSpaceRegistry.cpp:741`
    //  直接转发它）。
    const KoColorProfile *p709SRGB = KoColorSpaceRegistry::instance()->p709SRGBProfile();
    std::printf("PREMISE p709SRGBProfile=%s\n", p709SRGB ? "non-null" : "nullptr");
    if (p709SRGB) {
        std::printf("PREMISE p709SRGBProfile name=%s rawDataSize=%d\n",
                    p709SRGB->name().PkToUtf8().c_str(),
                    p709SRGB->rawData().size());
    }

    // 对照访问器（`profileFor(..., BT.2020, LINEAR)` 走它）：与 R-72 Task 1 交叉验证
    // （impact-map §3 待办 #1 的同一件事）。
    const KoColorProfile *p2020G10 = KoColorSpaceRegistry::instance()->p2020G10Profile();
    std::printf("PREMISE p2020G10Profile=%s\n", p2020G10 ? "non-null" : "nullptr");
    if (p2020G10) {
        std::printf("PREMISE p2020G10Profile name=%s rawDataSize=%d\n",
                    p2020G10->name().PkToUtf8().c_str(),
                    p2020G10->rawData().size());
    }

    // 4. 复刻 `:340-354`：`primaries == PRIMARIES_UNSPECIFIED` 时 `colorants` 为空。
    const PkVector<double> colorants; // 空（不参与 profileFor 前四个分支判定）

    // 复刻 `:356-358`（nclx 分支，primaries=BT.709、transfer=IEC_61966_2_1）。
    const KoColorProfile *profile =
        KoColorSpaceRegistry::instance()->profileFor(colorants,
                                                     PRIMARIES_ITU_R_BT_709_5,
                                                     TRC_IEC_61966_2_1);
    std::printf("PREMISE profileFor({}, PRIMARIES_ITU_R_BT_709_5, TRC_IEC_61966_2_1)=%s\n",
                profile ? "non-null" : "nullptr");
    if (profile) {
        std::printf("PREMISE profileFor(BT.709,IEC_61966_2_1).name=%s rawDataSize=%d\n",
                    profile->name().PkToUtf8().c_str(),
                    profile->rawData().size());
    }

    // 「转发」证据：`profileFor(..., BT.709, IEC_61966_2_1)` 即 `p709SRGBProfile()`
    // （`KoColorSpaceRegistry.cpp:741` 的 `return p709SRGBProfile();`）。二者应恒等
    // （资源齐备同指针；无资源同为空）。
    const bool forwarded = (profile == p709SRGB);
    std::printf("PREMISE profileFor(BT.709,IEC_61966_2_1) == p709SRGBProfile(): %s\n",
                forwarded ? "true" : "false");
    if (!forwarded) {
        std::printf("FAIL: profileFor(BT.709, IEC_61966_2_1) 未转发到 p709SRGBProfile()\n");
        ++failures;
    }

    // 对照：`HeifExport.cpp:202` 那条 `profileFor(..., BT.2020, TRC_LINEAR)` 的走法
    // （转发到 `p2020G10Profile()`）。这里只为把该访问器的返空钉住。
    const KoColorProfile *linear2020 =
        KoColorSpaceRegistry::instance()->profileFor(PkVector<double>(),
                                                     PRIMARIES_ITU_R_BT_2020_2_AND_2100_0,
                                                     TRC_LINEAR);
    std::printf("PREMISE profileFor({}, PRIMARIES_ITU_R_BT_2020_2_AND_2100_0, TRC_LINEAR)=%s\n",
                linear2020 ? "non-null" : "nullptr");
    if (linear2020) {
        std::printf("PREMISE profileFor(BT.2020,LINEAR).name=%s rawDataSize=%d\n",
                    linear2020->name().PkToUtf8().c_str(),
                    linear2020->rawData().size());
    }
    std::printf("PREMISE profileFor(BT.2020,LINEAR) == p2020G10Profile(): %s\n",
                (linear2020 == p2020G10) ? "true" : "false");

    if (unguardedShape) {
        // 崩溃时 stdout 缓冲会丢：先把前提刷出去，使「读到下面的 marker 后才崩」可观测。
        std::printf("UNGUARDED-SHAPE about to dereference profile (HeifImport.cpp:365 原样: "
                    "dbgFile << \"nclx profile found\" << profile->name())\n");
        std::fflush(stdout);
        // ——— 修前形态（裁决 B 改动**之前**，`HeifImport.cpp:365` 的原样）：
        //     `dbgFile << "nclx profile found" << profile->name();`
        //     无 EXTRA_RESOURCE_DIRS 时 profile==nullptr ⇒ `->name()` 当场解引用空指针。
        //     用返回值的长度驱动 printf，防编译器把这次解引用优化掉（否则「没崩」可能只是
        //     「没执行」）。
        const std::size_t nameLen = profile->name().size();
        std::printf("UNGUARDED-SHAPE reached profile->name(), nameLen=%zu "
                    "(读到这行 = 解引用没崩)\n",
                    nameLen);
        std::fflush(stdout);
        return 0;
    }

    // ——— 受检形态（裁决 B 改动**后**的形状）：
    //     `dbgFile << "nclx profile found"
    //              << (profile ? profile->name()
    //                          : PkString("(unavailable: no resource dirs)"));`
    //     profile 为空是降级环境下的**正常结果**（下面 `:388` 的 `if (!profile)` 本来就会
    //     去取默认剖面），故这里**不判失败**，只如实打印——不改导入行为、不替宿主兜底
    //     （R-59 裁决 C）。
    std::printf("CHECKED-SHAPE profile=%s\n", profile ? "non-null" : "nullptr");
    const PkString nclxName =
        profile ? profile->name() : PkString("(unavailable: no resource dirs)");
    std::printf("CHECKED-BRANCH nclx profile found %s\n", nclxName.PkToUtf8().c_str());

    // 5. 走到这里 = 进程没崩（修前形态在无资源环境下会当场解引用 nullptr）。
    //    崩溃时 stdout 缓冲会丢，所以上面每一步都刻意打印、事后取证用。
    std::fflush(stdout);
    std::printf("RESULT failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
