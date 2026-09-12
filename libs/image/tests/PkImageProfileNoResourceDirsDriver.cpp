/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// ============================================================================
// ⚠ 这是「复刻调用点形状的 driver」，不是测试文件。
//
// **这不是 `libs/image/tests/kis_image_test.cpp`，是复刻它的
// `KisImageTest::testAssignImageProfile`（该文件 `:214` 起）调用点形状的 driver**
// ——因为真实测试类在本 worktree 里物理编不过。
//
// 依赖墙（实测，Task 1 Step 1 现场重跑复现；原始输出见 task1-report.md「Step 1」）：
//   kis_image_test.cpp:315:38: error: no matching constructor for initialization of 'KisAnnotation'
//   :335/:336/:344/:345/:368/:369/:425/:426: error: call to 'findNode' is ambiguous
//   :338/:339/:350/:351/:374/:375: error: no viable conversion from 'PkNodeId' to 'QUuid'
//   :502/:504/:507: error: no matching member function for call to 'fill'
//   :505:37: error: no viable conversion from 'QRect' to 'const PkRect'
//   归属：KisAnnotation 构造签名 · findNode 重载歧义 · PkNodeId↔QUuid · fill 重载与
//   QRect→PkRect —— 全部是 S 线 `libs/image` 的源码迁移残留；「整目标编不过」这一条
//   归 **R-65**（「测试/benchmark 侧 163 个 target 编不过」）。
//
// 这堵墙什么时候会被拆掉：S 线完成 `libs/image` 的源码迁移 + R-65 收口测试侧 target。
// 届时本 driver 可退役，改回编译真实测试类
// （`libs/image/tests/CMakeLists.txt` 已用 `kis_add_tests(...)` 注册 `kis_image_test.cpp`）。
//
// 本 driver 逐行复刻的真实调用形状（`kis_image_test.cpp:216–260`）：
//   - `const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();`
//   - `new KisImage(0, 1000, 1000, rgb8, "stest")`
//   - `*image->colorSpace()->profile()` 与
//     `*KoColorSpaceRegistry::instance()->p709SRGBProfile()` 做 `QCOMPARE` 的
//     **同一对操作数**（QCOMPARE 展开为 `KoColorProfile::operator==`；对应 `:237`）；
//   - `*image->colorSpace()->profile()` 与
//     `*KoColorSpaceRegistry::instance()->p2020G10Profile()` 做 `QCOMPARE` 的
//     **同一对操作数**（对应 `:252`）。
//   复刻范围内只含这条最小链（`rgb8()`→`KisImage`→剖面比较），不含 `:218` 之后的
//   `KisPaintLayer`/`KisAdjustmentLayer` 部分——本任务要证的是「返空访问器被解引用
//   会崩」，那条链已足够命中，且不引入与 p709/p2020 无关的构造前置。
//
// **已声明的收窄（措辞不超出探针喂过的输入集合）**：真实测试里 6 处 `QCOMPARE` 的
//   **接收者**分两组——`image->colorSpace()->profile()`（`:237`/`:252`）与
//   `paint1->…`/`blur1->…`（`:240`/`:245`/`:255`/`:260`）。本 driver **只复刻
//   `image->colorSpace()->profile()` 这一组接收者**（两个访问器各一条：p709 与 p2020
//   并列），`paint1`/`blur1` 那两个变体是**同一表达式形状的同形实例**（仅接收者指针
//   不同，对返空访问器返回值的解引用机理逐字相同），未另立分支——在此如实收窄。
//   `:248` 的 `assignImageProfile(…->p2020G10Profile())` 是**非**解引用点
//   （`KisImage::assignImageProfile` 首行 `if (!profile) return false;`），不在范围。
// 校验值全部来自本进程对真实内核的实测打印，不猜。
//
// 命令行开关：
//   （无参数）            默认**无条件** `unsetenv("EXTRA_RESOURCE_DIRS")`，把进程钉在
//                         「无资源」条件（封闭，ctest 路径不随调用者环境漂移）。
//   --keep-resource-dirs  保留调用者设的 `EXTRA_RESOURCE_DIRS`（资源齐备对照组）。
//   --unguarded-shape     走**修前形态**（R-72 裁决 A 改动**之前**、`:237` 的原样）：
//                         直接解引用右侧访问器的返回值、不做判空。无
//                         `EXTRA_RESOURCE_DIRS` 下期望 SIGSEGV（退出码 139）。
// ============================================================================

#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <LcmsEnginePlugin.h>

#include "kis_image.h"

#include <PkString.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

int failures = 0;

} // namespace

int main(int argc, char **argv)
{
    // 1. 解析开关。默认把进程钉死在「无资源」条件（与 R-59/R-61 常驻载体同款；放最前）。
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

    // 被复刻的调用点一侧：`KoColorSpaceRegistry::instance()->p709SRGBProfile()`。
    const KoColorProfile *elleSrgb = KoColorSpaceRegistry::instance()->p709SRGBProfile();
    std::printf("PREMISE p709SRGBProfile=%s\n", elleSrgb ? "non-null" : "nullptr");
    if (elleSrgb) {
        std::printf("PREMISE p709SRGBProfile name=%s rawDataSize=%d\n",
                    elleSrgb->name().PkToUtf8().c_str(),
                    elleSrgb->rawData().size());
    }

    // 第二个返空访问器（impact-map §3 待办 #1：此前只有读码结论，这里用探针钉住）。
    const KoColorProfile *elleRec2020G10 = KoColorSpaceRegistry::instance()->p2020G10Profile();
    std::printf("PREMISE p2020G10Profile=%s\n", elleRec2020G10 ? "non-null" : "nullptr");
    if (elleRec2020G10) {
        std::printf("PREMISE p2020G10Profile name=%s rawDataSize=%d\n",
                    elleRec2020G10->name().PkToUtf8().c_str(),
                    elleRec2020G10->rawData().size());
    }

    // 复刻 `:216` 的 `rgb8()`（它同时是 `new KisImage(..., rgb8, ...)` 的入参）。
    const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();
    if (!rgb8) {
        std::printf("FATAL rgb8()==nullptr：无法复刻调用形状\n");
        std::fflush(stdout);
        return 2;
    }
    std::printf("PREMISE default rgb8 profile name=%s rawDataSize=%d\n",
                rgb8->profile() ? rgb8->profile()->name().PkToUtf8().c_str() : "(null)",
                rgb8->profile() ? rgb8->profile()->rawData().size() : -1);

    // 复刻 `:216` 的 `new KisImage(0, 1000, 1000, rgb8, "stest")`。
    KisImageSP image = new KisImage(0, 1000, 1000, rgb8, "stest");
    if (!image || !image->colorSpace() || !image->colorSpace()->profile()) {
        std::printf("FATAL KisImage 构造后 colorSpace()/profile() 为空\n");
        std::fflush(stdout);
        return 2;
    }
    std::printf("PREMISE KisImage colorSpace profile name=%s\n",
                image->colorSpace()->profile()->name().PkToUtf8().c_str());

    if (unguardedShape) {
        // 崩溃时 stdout 缓冲会丢：先把前提刷出去，使「读到下面的 marker 后才崩」可观测。
        std::printf("UNGUARDED-SHAPE about to dereference "
                    "KoColorSpaceRegistry::instance()->p709SRGBProfile()\n");
        std::fflush(stdout);
        // ——— 修前形态（裁决 A 改动**之前**，`kis_image_test.cpp:237` 的原样）：
        //     `QCOMPARE(*image->colorSpace()->profile(),
        //               *KoColorSpaceRegistry::instance()->p709SRGBProfile());`
        //     右侧**直接解引用**；无 EXTRA_RESOURCE_DIRS 时 p709SRGBProfile()==nullptr
        //     ⇒ 当场 SIGSEGV。用 volatile 承接结果，防编译器把这次解引用优化掉
        //     （否则「没崩」可能只是「没执行」）。
        volatile bool equal = (*image->colorSpace()->profile() ==
                               *KoColorSpaceRegistry::instance()->p709SRGBProfile());
        std::printf("UNGUARDED-SHAPE reached the comparison, equal=%d "
                    "(读到这行 = 解引用没崩)\n",
                    equal ? 1 : 0);
        std::fflush(stdout);

        // ——— p2020 那一对（`kis_image_test.cpp:252` 的修前形态，与上面 p709 并列）：
        //     `QCOMPARE(*image->colorSpace()->profile(),
        //               *KoColorSpaceRegistry::instance()->p2020G10Profile());`
        //     同一接收者表达式，右侧同样**直接解引用**。无 EXTRA_RESOURCE_DIRS 时
        //     p2020G10Profile() 亦为 nullptr ⇒ 同样当场 SIGSEGV（与 p709 那处**机理同形**：
        //     对返空访问器返回值做 `*`）。⚠ 上面 p709 那处先崩，故本行在**无资源的
        //     `--unguarded-shape`** 用例里读不到——本 driver 以同形并列表达「6 处调用点
        //     形状」；本行可在 `--unguarded-shape --keep-resource-dirs` 下被走到（那时
        //     两访问器非空、不崩）。
        std::printf("UNGUARDED-SHAPE about to dereference "
                    "KoColorSpaceRegistry::instance()->p2020G10Profile()\n");
        std::fflush(stdout);
        volatile bool equal2020 = (*image->colorSpace()->profile() ==
                                   *KoColorSpaceRegistry::instance()->p2020G10Profile());
        std::printf("UNGUARDED-SHAPE reached the comparison, equal=%d "
                    "(读到这行 = 解引用没崩)\n",
                    equal2020 ? 1 : 0);
        std::fflush(stdout);
        return 0;
    }

    // ——— 受检形态（裁决 A 改动**后**的形状）：两个访问器各取一次受检局部量，
    //     任一为空 ⇒ 前提不成立，明确报失败、**不比较**
    //     （对照裁决 A 的 `PK_VERIFY2(false, PkString(...).PkToUtf8())`；该宏的失败分支
    //     就是 `return;`，故真实测试里到此为止）。
    std::printf("CHECKED-SHAPE elleSrgb=%s elleRec2020G10=%s\n",
                elleSrgb ? "non-null" : "nullptr",
                elleRec2020G10 ? "non-null" : "nullptr");
    if (!elleSrgb || !elleRec2020G10) {
        std::printf("CHECKED-BRANCH premise does NOT hold: "
                    "p709SRGBProfile()/p2020G10Profile() == nullptr "
                    "=> testAssignImageProfile 的前提（rgb8() 携带 elle sRGB，"
                    "且 assignImageProfile(elle Rec2020 g10) 往返）不成立，"
                    "受检分支走到「明确报失败」这一支"
                    "（对照裁决 A 的 PK_VERIFY2(false, PkString(...).PkToUtf8())）\n");
        std::fflush(stdout);
        std::printf("RESULT failures=%d\n", failures);
        return failures == 0 ? 0 : 1;
    }

    // 前提成立：复刻 `:237` 的**同一对操作数**比较。
    // （`image->colorSpace()` 由 `rgb8` 给出 ⇒ 与 `p709SRGBProfile()` 同源；资源齐备时
    //  两者应为同一剖面。这里如实记录，不预设结论。）
    const KoColorProfile *deviceProfile = image->colorSpace()->profile();
    std::printf("COMPARE deviceProfile name=%s elleSrgb name=%s\n",
                deviceProfile->name().PkToUtf8().c_str(),
                elleSrgb->name().PkToUtf8().c_str());
    const bool equal = (*deviceProfile == *elleSrgb);
    std::printf("COMPARE *deviceProfile == *p709SRGBProfile(): %s\n",
                equal ? "true" : "false");
    if (!equal) {
        std::printf("FAIL: image->colorSpace()->profile() != p709SRGBProfile() under "
                    "resource dirs\n");
        ++failures;
    }

    // 复刻 `:252` 的**同一对操作数**比较（p2020 那一对，与上面 p709 并列）：接收者同为
    // `image->colorSpace()->profile()`，另一侧是受检局部量 `elleRec2020G10`
    // （对应 `:252` 的 `*KoColorSpaceRegistry::instance()->p2020G10Profile()`）。
    // ⚠ 本 driver 不做 `:248` 的 `assignImageProfile(elle Rec2020 g10)` 往返（文件头已
    //   声明这一收窄），故 receiver 仍是 rgb8 派生的剖面，与 elleRec2020G10 **不同**——
    //   如实记录该比较结果，**不置失败**（要让它相等就得走被排除的往返段）。
    std::printf("COMPARE deviceProfile name=%s elleRec2020G10 name=%s\n",
                deviceProfile->name().PkToUtf8().c_str(),
                elleRec2020G10->name().PkToUtf8().c_str());
    const bool equal2020 = (*deviceProfile == *elleRec2020G10);
    std::printf("COMPARE *deviceProfile == *p2020G10Profile(): %s "
                "(receiver 未做 assignImageProfile 往返 ⇒ 预期 false，仅记录、不判失败)\n",
                equal2020 ? "true" : "false");

    // 5. 走到这里 = 进程没崩（修前形态在无资源环境下会当场解引用 nullptr）。
    //    崩溃时 stdout 缓冲会丢，所以上面每一步都刻意打印、事后取证用。
    std::fflush(stdout);
    std::printf("RESULT failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
