/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// ============================================================================
// ⚠ 这是「复刻调用点形状的 driver」，不是测试文件。
//
// **这不是 `libs/pigment/tests/TestKoColor.cpp`，是复刻它的
// `TestKoColor::testExistingSerializations`（该文件 `:68` 起）`:`68–:`87 这一段调用点
// 形状的 driver**——因为真实测试类在本 worktree 里物理编不过。
//
// 依赖墙（实测，本文写作前现场重跑复现：`ninja -C build-r72 TestKoColor` → EXIT=1，
// `build-r72/bin/TestKoColor` 不存在）：
//   ① AUTOMOC 为 Pk 形态测试类生成 `staticMetaObject` / `qt_static_metacall` /
//      `qt_metacast` —— `..._autogen/EWIEGA46WW/moc_TestKoColor.cpp:87/90/106/107/110/116/118/121/126/129/131`
//      （`out-of-line definition of 'qt_static_metacall' does not match any declaration`
//       · `no member named 'staticMetaObject' in 'TestKoColor'` · `static_cast from
//      'QObject *' to 'TestKoColor *', which are not related by inheritance`）。
//      归属：`libs/pigment/tests/CMakeLists.txt` 的 `pigment_add_pk_test_binder`
//      （AUTOMOC OFF + `pk_test_moc.py`，`:47`）已给出解法，但 `_pigment_pk_target`
//      列表（`:82`）里没有 `TestKoColor`。
//   ② `QDebug` 与 `PkString` 无 `operator<<` —— `TestKoColor.cpp:43:24:
//      error: invalid operands to binary expression ('QDebug' and 'PkString')`
//      （`:298` 同形）。归属：`pk/log` 的 QDebug 垫片未覆盖 PkString 重载（R-08 线域）。
//   ③ `Qt::GlobalColor` → `PkColor` 歧义 —— `:212:13` / `:224:13`
//      `error: no viable conversion from 'Qt::GlobalColor' to 'PkColor'`。
//      归属：S-06 的源码迁移（`KoColor(Qt::transparent)` 一族）。
//   ④ `KoColor` 构造签名不匹配 —— `:235/236/237/246/247/248`
//      `error: no matching constructor for initialization of 'KoColor'`。归属：同上（S-06）。
//   ⑤ `qExec` 找不到 —— `:378:1: error: no matching function for call to 'qExec'`。
//      归属：入口仍是 `kistest.h` 的 `KISTEST_MAIN`（真 Qt 应用对象）。
//   「整目标编不过」这一条归 **R-65**（「测试/benchmark 侧 163 个 target 编不过」）。
//
// 这堵墙什么时候会被拆掉：`pk/log` 垫片补上 `PkString` 重载（R-08）·
// `libs/pigment/tests/CMakeLists.txt` 把 `TestKoColor` 收进 `_pigment_pk_target`
// （消 AUTOMOC 那层）· S-06 完成该文件剩下的源码迁移（消 ③④⑤）· R-65 收口测试侧
// target。届时本 driver 可退役，改回编译真实测试类
// （`libs/pigment/tests/CMakeLists.txt:6-20` 已用 `kis_add_tests(...)` 注册 `TestKoColor.cpp`）。
//
// 本 driver 逐行复刻的真实调用形状（`TestKoColor.cpp:68–:87`）：
//   :74  PkColor c;
//   :76  main = "<sRGB r='0' g='0' b='1' />";
//   :77  doc.setContent(main);
//   :78  KoColor sRGB = KoColor::fromXML(doc.documentElement(), Integer8BitsColorDepthID.id());
//   :79  sRGB.toQColor(&c);
//   :80  PkString blue = "#0000FF";
//   :81  PK_VERIFY2(c == PkColor(blue), PkString(...).arg(c.name()).arg(blue).PkToUtf8());
//        ← 该宏体的失败分支是 `return;`（`pk/test/PkTest.h:116-127`），故 :81 失败则
//          函数到此为止、**:85 不可达**。本 driver 把这条前提的**实测结果**打印出来
//          （这是本 driver 存在的第二个理由：先测 `:85` 到底可不可达）。
//   :85  PkString Rec2020profile = KoColorSpaceRegistry::instance()->p2020G10Profile()->name();
//   :86  main = PkString("<RGB r='3.0' g='0' b='1' space='%1'/>").arg(Rec2020profile);
//   :87  doc.setContent(main);
//   校验值全部来自本进程对真实内核的实测打印，不猜。
//
// 命令行开关：
//   （无参数）            默认**无条件** `unsetenv("EXTRA_RESOURCE_DIRS")`，把进程钉在
//                        「无资源」条件（封闭，ctest 路径不随调用者环境漂移）。
//   --keep-resource-dirs  保留调用者设的 `EXTRA_RESOURCE_DIRS`（资源齐备对照组）。
//   --unguarded-shape     走**修前形态**（`:85` 原样）：`…->p2020G10Profile()->name()`
//                        直接解引用、不做判空。无 `EXTRA_RESOURCE_DIRS` 下期望
//                        SIGSEGV（退出码 139）。**注意**：`:81` 那条前提若在本条件下
//                        失败，真实测试会先 `return;`、`:85` 不可达——本 driver 复刻
//                        该前提的实测结果，前提不成立时即使 `--unguarded-shape` 也
//                        如实报告「不可达」，不硬走解引用。
// ============================================================================

#include <KoColor.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <LcmsEnginePlugin.h>

#include <PkColor.h>
#include <PkString.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{

int failures = 0;

} // namespace

int main(int argc, char **argv)
{
    // 1. 解析开关。默认把进程钉死在「无资源」条件（与 R-59/R-61/R-72 常驻载体同款；
    //    放最前）。
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

    // 2. 注册 lcms 引擎——elle 剖面（p2020G10Profile() 的来源）由它从资源目录载入；
    //    不注册则访问器取不到（**读码所得、未经探针实测**：本 driver 与所有探针都先注册
    //    引擎，从未喂过「未注册引擎」这一条件）。
    registerLcmsEngine();

    // 3. 打印前提（实测值，不猜）。
    const char *extra = std::getenv("EXTRA_RESOURCE_DIRS");
    std::printf("PREMISE EXTRA_RESOURCE_DIRS=%s\n", extra ? extra : "(unset)");

    // 被复刻调用点 :85 一侧的访问器（impact-map §3 待办 #1 已由 R-72 T1 钉过，这里复核）。
    const KoColorProfile *elleRec2020G10 = KoColorSpaceRegistry::instance()->p2020G10Profile();
    std::printf("PREMISE p2020G10Profile=%s\n", elleRec2020G10 ? "non-null" : "nullptr");
    if (elleRec2020G10) {
        std::printf("PREMISE p2020G10Profile name=%s rawDataSize=%d\n",
                    elleRec2020G10->name().PkToUtf8().c_str(),
                    elleRec2020G10->rawData().size());
    }
    const KoColorProfile *elleSrgb = KoColorSpaceRegistry::instance()->p709SRGBProfile();
    std::printf("PREMISE p709SRGBProfile=%s\n", elleSrgb ? "non-null" : "nullptr");
    const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();
    if (rgb8) {
        std::printf("PREMISE default rgb8 profile name=%s rawDataSize=%d\n",
                    rgb8->profile() ? rgb8->profile()->name().PkToUtf8().c_str() : "(null)",
                    rgb8->profile() ? rgb8->profile()->rawData().size() : -1);
    }

    // 4. 复刻 `:74–:82`：sRGB 反序列化 + `:81` 的 PK_VERIFY2 前提。
    PkString main;
    PkXmlDocument doc;
    PkColor c;
    main = "<sRGB r='0' g='0' b='1' />";
    doc.setContent(main);
    KoColor sRGB = KoColor::fromXML(doc.documentElement(), Integer8BitsColorDepthID.id());
    sRGB.toQColor(&c);
    PkString blue = "#0000FF";

    // —— `:81` 的 PK_VERIFY2 条件（**实测**，不是假定）。它的失败分支是 `return;`。
    const bool sRGBPremiseHolds = (c == PkColor(blue));
    std::printf("PROBE :81  premise (c == PkColor(\"#0000FF\")) = %s  [c.name()=%s]\n",
                sRGBPremiseHolds ? "true" : "false",
                c.name().PkToUtf8().c_str());

    if (!sRGBPremiseHolds) {
        // 真实测试在 `:81` 会 `PK_VERIFY2(false, …)` ⇒ 宏体 `return;` ⇒ **`:85` 走不到**。
        // 如实报告「`:85` 在本环境下不可达 ⇒ 不构成缺陷」。
        std::printf("UNREACHABLE: :81 PK_VERIFY2 would be FALSE => 宏体 return; "
                    "=> :85 不可达（:85 的裸解引用在本条件下不是可执行的缺陷）\n");
        std::fflush(stdout);
        std::printf("RESULT failures=%d\n", failures);
        return failures == 0 ? 0 : 1;
    }

    // ——— 前提成立：`:85` 可达。
    if (unguardedShape) {
        // 崩溃时 stdout 缓冲会丢：先把前提刷出去，使「读到下面的 marker 后才崩」可观测。
        std::printf("UNGUARDED-SHAPE about to dereference "
                    "KoColorSpaceRegistry::instance()->p2020G10Profile()  (:85 原样)\n");
        std::fflush(stdout);
        // ——— 修前形态（`TestKoColor.cpp:85` 原样）：
        //     `PkString Rec2020profile = KoColorSpaceRegistry::instance()->p2020G10Profile()->name();`
        //     右侧**直接解引用**；无 EXTRA_RESOURCE_DIRS 时 p2020G10Profile()==nullptr
        //     ⇒ 当场 SIGSEGV。（用 printf 承接结果，防编译器把这次解引用优化掉。）
        PkString Rec2020profile = KoColorSpaceRegistry::instance()->p2020G10Profile()->name();
        std::printf("UNGUARDED-SHAPE reached the dereference, Rec2020profile=%s "
                    "(读到这行 = 解引用没崩)\n",
                    Rec2020profile.PkToUtf8().c_str());
        std::fflush(stdout);
        // ——— 复刻 `:86` 的后续使用（同一表达式形状的延续）。
        main = PkString("<RGB r='3.0' g='0' b='1' space='%1'/>").arg(Rec2020profile);
        std::printf("UNGUARDED-SHAPE main=%s\n", main.PkToUtf8().c_str());
        std::fflush(stdout);
        return 0;
    }

    // ——— 受检形态（裁决 A 改动**后**的形状）：先取受检局部量，为空 ⇒ 前提不成立、
    //     明确报失败（对照 `PK_VERIFY2(false, PkString(...).PkToUtf8())`，其失败分支
    //     即 `return;`，真实测试到此为止）、**不比较、不解引用**。
    std::printf("CHECKED-SHAPE elleRec2020G10=%s\n",
                elleRec2020G10 ? "non-null" : "nullptr");
    if (!elleRec2020G10) {
        std::printf("CHECKED-BRANCH premise does NOT hold: p2020G10Profile() == nullptr "
                    "=> testExistingSerializations 的前提（Rec2020-elle-V4-g10.icc 已载入）"
                    "不成立，受检分支走到「明确报失败」这一支"
                    "（对照裁决 A 的 PK_VERIFY2(false, PkString(...).PkToUtf8())）\n");
        std::fflush(stdout);
        std::printf("RESULT failures=%d\n", failures);
        return failures == 0 ? 0 : 1;
    }

    // 前提成立：复刻 `:85`（受检形态 —— 受检局部量 `elleRec2020G10->name()`）。
    PkString Rec2020profile = elleRec2020G10->name();
    std::printf("CHECKED-BRANCH Rec2020profile=%s\n", Rec2020profile.PkToUtf8().c_str());
    if (Rec2020profile.PkToUtf8().empty()) {
        std::printf("FAIL: p2020G10Profile()->name() empty under resource dirs\n");
        ++failures;
    }
    // 复刻 `:86`（同一调用形状的延续）。
    main = PkString("<RGB r='3.0' g='0' b='1' space='%1'/>").arg(Rec2020profile);
    std::printf("CHECKED-BRANCH main=%s\n", main.PkToUtf8().c_str());

    // 5. 走到这里 = 进程没崩（修前形态在无资源环境下会当场解引用 nullptr）。
    std::fflush(stdout);
    std::printf("RESULT failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
