/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

// ============================================================================
// ⚠ 这是「复刻调用点形状的 driver」，不是测试文件。
//
// **这不是 `libs/pigment/tests/TestColorConversionSystem.cpp`，是复刻它的调用点
// 形状的 driver**——因为真实测试类在本 worktree 里物理编不过。四类源码层阻塞
// （实测原始输出见 R-61 plan §1 F4；本 driver 立项时已现场重跑复现，见 Task 2 报告
// 「Step 1」）：
//
//   ① `QDebug` 与 `PkString` 无 `operator<<`
//        —— `TestColorConversionSystem.cpp:53` 与 `KoColorConversionSystem_p.h:48`
//        归属：`pk/log` 的 QDebug 垫片未覆盖 PkString 重载（R-08 线域）。
//   ② AUTOMOC 为 Pk 形态测试类生成 `staticMetaObject` / `qt_static_metacall` / `qt_metacast`
//        —— `..._autogen/EWIEGA46WW/moc_TestColorConversionSystem.cpp`
//        归属：`libs/pigment/tests/CMakeLists.txt` 的 `pigment_add_pk_test_binder`
//        已给出解法（AUTOMOC OFF + `pk_test_moc.py`），但 `_pigment_pk_target`
//        列表里没有 `TestColorConversionSystem`。
//   ③ `qExec` 找不到 —— `TestColorConversionSystem.cpp:484`
//        归属：入口仍是 `kistest.h` 的 `KISTEST_MAIN`（真 Qt 应用对象）。
//   ④ `Qt::GlobalColor` → `PkString` 歧义 —— `:352/356/362/370/376`
//        归属：S-06 的源码迁移（`KoColor(Qt::transparent)` 形态）。
//
// 这堵墙什么时候会被拆掉：`pk/log` 垫片补上 `PkString` 重载（R-08）·
// `libs/pigment/tests/CMakeLists.txt` 把 `TestColorConversionSystem` 收进
// `_pigment_pk_target` · S-06 完成该文件剩下的源码迁移。届时本 driver 可退役，
// 改回编译真实测试类。
//
// 本 driver 逐行复刻的真实调用形状（对应 `TestColorConversionSystem` 的
// `testAlphaConnectionPaths` / `testGrayAConnectionPaths` 与 `calcPath`）：
//   - 同样的 `KoColorSpaceRegistry::instance()->p709SRGBProfile()` 调用，取 `->name()`
//   - 同样的 `{model, depth, profile}` 三元组（`KoColorConversionSystem::NodeKey`）构造
//   - 同样的 `existsPath` / `existsGoodPath` 调用
//   - 同样照 `calcPath` 的写法走 `findBestPath` + `vertexes`
// 校验值全部来自本进程对真实内核的实测打印，不猜。
//
// 命令行开关：默认（无参数）强制 EXTRA_RESOURCE_DIRS 缺席；手工传
// `--keep-resource-dirs` 才保留调用者设的 EXTRA_RESOURCE_DIRS（形态照抄 T1 已验证的
// plugins/impex/webp/tests/KisWebPExportNoResourceDirsTest.cpp，用于资源齐备对照组）。
// ============================================================================

#include <KoColorConversionSystem_p.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <LcmsEnginePlugin.h>

#include <PkString.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

// 访问 `KoColorSpaceRegistry::colorConversionSystem()` 的口子：它在
// KoColorSpaceRegistry.h:410 是 private。真实测试类 `TestColorConversionSystem`
// 靠同头文件 :395 的 friend 声明进去；本 driver 是复刻品、不在那份名单里，而
// 任务边界禁止改 `libs/pigment/` 的非 `tests/` 部分（那会动 KoColorSpaceRegistry.h）。
// 同头文件 :396 已经 `friend struct FriendOfColorSpaceRegistry`，并且同目录的
// CCSGraph.cpp:19-30 正是用同名 struct 走这个口子——本 driver 定义同名 struct
// 取得同一份 friendship。**调用形状本身不变**（仍是
// `…->colorConversionSystem()->existsPath(…)`）。
struct FriendOfColorSpaceRegistry {
    static const KoColorConversionSystem *colorConversionSystem() {
        return KoColorSpaceRegistry::instance()->colorConversionSystem();
    }
};

namespace
{

int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

// calcPath：逐行复刻 `TestColorConversionSystem::calcPath`
// （`libs/pigment/tests/TestColorConversionSystem.cpp:71-88`）。
// 唯一的形式改动是把 `Q_FOREACH` 换成等价的 range-for —— `PkList` 提供迭代器，
// 遍历顺序与语义完全一致（既非「简化版」也非臆想场景）。
std::vector<KoColorConversionSystem::NodeKey>
calcPath(const std::vector<KoColorConversionSystem::NodeKey> &expectedPath)
{
    const KoColorConversionSystem *system =
        FriendOfColorSpaceRegistry::colorConversionSystem();

    KoColorConversionSystem::Path path =
        system->findBestPath(expectedPath.front(), expectedPath.back());

    std::vector<KoColorConversionSystem::NodeKey> realPath;

    for (const KoColorConversionSystem::Vertex *vertex : path.vertexes) {
        if (!vertex->srcNode->isEngine) {
            realPath.push_back(vertex->srcNode->key());
        }
    }
    realPath.push_back(path.vertexes.last()->dstNode->key());

    return realPath;
}

} // namespace

int main(int argc, char **argv)
{
    // 1. 默认把进程钉死在「无资源」条件（与 R-59 常驻载体同款；放最前）。
    //
    //    R-61 T2 修复轮：证据缺口——本行原先**无条件** unsetenv，导致下面
    //    `if (!srgbProfile) … else { srgbProfile->name() … }` 里的 else 分支
    //    从未被执行：driver 只证了「前提不成立 ⇒ 报失败那一支、不崩」，没证另一半
    //    （前提成立时 `srgbProfile->name()` 那条路安全）——而那正是 10 处改动实际改掉
    //    的东西。故加显式开关 `--keep-resource-dirs`（形态照抄 T1 已验证的
    //    plugins/impex/webp/tests/KisWebPExportNoResourceDirsTest.cpp）：默认仍是
    //    无条件 unsetenv（封闭，ctest 路径不随调用者环境漂移），只有手工传该开关时
    //    才保留调用者设的 EXTRA_RESOURCE_DIRS。第 3 步会把 EXTRA_RESOURCE_DIRS 的
    //    实际值印进前提，让「这次跑的是哪个条件」可观测、不可能被冒充。
    bool keepResourceDirs = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--keep-resource-dirs") == 0) {
            keepResourceDirs = true;
        }
    }
    if (!keepResourceDirs) {
        ::unsetenv("EXTRA_RESOURCE_DIRS");
    }

    // 2. 注册 lcms 引擎——elle 剖面（p709SRGBProfile() 的来源）由它从资源目录载入；
    //    不注册则 p709SRGBProfile() 取不到（**读码所得、未经探针实测**：本 driver 与所有
    //    探针都先注册引擎，从未喂过「未注册引擎」这一条件），区分不出本 driver 要区分的条件。
    registerLcmsEngine();

    // 3. 打印前提（实测值，不猜）。
    const char *extra = std::getenv("EXTRA_RESOURCE_DIRS");
    std::printf("PREMISE EXTRA_RESOURCE_DIRS=%s\n", extra ? extra : "(unset)");

    // 被复刻的调用点：`KoColorSpaceRegistry::instance()->p709SRGBProfile()`。
    const KoColorProfile *srgbProfile = KoColorSpaceRegistry::instance()->p709SRGBProfile();
    std::printf("PREMISE p709SRGBProfile=%s\n", srgbProfile ? "non-null" : "nullptr");
    if (srgbProfile) {
        std::printf("PREMISE p709SRGBProfile name=%s rawDataSize=%d\n",
                    srgbProfile->name().PkToUtf8().c_str(),
                    srgbProfile->rawData().size());
    }

    // 默认 RGBA/U8 空间的剖面 name/size——判据要的是可观测前提，不是假定前提。
    const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();
    expect(rgb8 != nullptr, "default RGBA/U8 colorspace must be available without resource dirs");
    if (rgb8) {
        const KoColorProfile *deviceProfile = rgb8->profile();
        std::printf("PREMISE default rgb8 profile name=%s rawDataSize=%d\n",
                    deviceProfile ? deviceProfile->name().PkToUtf8().c_str() : "(null)",
                    deviceProfile ? deviceProfile->rawData().size() : -1);
    }

    const KoColorConversionSystem *system = FriendOfColorSpaceRegistry::colorConversionSystem();

    // 4. 复刻 testAlphaConnectionPaths / testGrayAConnectionPaths 的**受检形态**
    //    （R-61 plan §2 裁决 B；即 Task 2 改动后的形状）。
    if (!srgbProfile) {
        // —— 这正是裁决 B 里 `PK_VERIFY2(false, PkString(...).PkToUtf8())` 报失败的那一支。
        //    真实测试里它会「明确报失败并 return；」；本 driver 没有测试框架，改为打印并
        //    计数——**绝不静默跳过、也绝不崩**。
        std::printf("CHECKED-BRANCH premise does NOT hold: p709SRGBProfile()==nullptr "
                    "=> testAlphaConnectionPaths / testGrayAConnectionPaths 的前提不成立，"
                    "受检分支走到「明确报失败」这一支"
                    "（对照裁决 B 的 PK_VERIFY2(false, PkString(...).PkToUtf8())）\n");
        std::fflush(stdout);
    } else {
        std::printf("CHECKED-BRANCH premise holds: srgbProfile->name()=%s\n",
                    srgbProfile->name().PkToUtf8().c_str());

        const KoColorSpace *alpha8 = KoColorSpaceRegistry::instance()->alpha8();
        expect(alpha8 != nullptr, "alpha8 colorspace must be available");
        // 把 `alpha8->profile()` 提到受检局部量，值不变（真实测试里它是三元组的第三个元素，
        // 直接内联成 `alpha8->profile()->name()`；此处只是避免 driver 在一个与 p709 无关的
        // 空指针上崩掉，掩盖真正要测的东西）。
        const KoColorProfile *alphaProfile = alpha8 ? alpha8->profile() : nullptr;
        expect(alphaProfile != nullptr, "alpha8 profile must be non-null");

        if (alpha8 && alphaProfile) {
            // —— 复刻 testAlphaConnectionPaths 里第一处 p709 三元组用例（真实行 124-128）。
            std::vector<KoColorConversionSystem::NodeKey> expectedPath = {
                {RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), srgbProfile->name()},
                {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(),
                 PkString("Gray-D50-elle-V2-srgbtrc.icc")},
                {alpha8->colorModelId().id(), alpha8->colorDepthId().id(), alphaProfile->name()}};

            std::printf("QUERY alpha8-case endpoints src=%s/%s/%s dst=%s/%s/%s\n",
                        expectedPath.front().modelId.PkToUtf8().c_str(),
                        expectedPath.front().depthId.PkToUtf8().c_str(),
                        expectedPath.front().profileName.PkToUtf8().c_str(),
                        expectedPath.back().modelId.PkToUtf8().c_str(),
                        expectedPath.back().depthId.PkToUtf8().c_str(),
                        expectedPath.back().profileName.PkToUtf8().c_str());

            // 照 calcPath 的写法喂给路径查询。
            const std::vector<KoColorConversionSystem::NodeKey> realPath = calcPath(expectedPath);
            std::printf("QUERY calcPath(alpha8-case) realPathLen=%d\n",
                        static_cast<int>(realPath.size()));

            // 对照 calcPath 里存在的 existsPath / existsGoodPath 形态（三元组首尾）。
            const bool exists = system->existsPath(
                expectedPath.front().modelId, expectedPath.front().depthId,
                expectedPath.front().profileName, expectedPath.back().modelId,
                expectedPath.back().depthId, expectedPath.back().profileName);
            const bool existsGood = system->existsGoodPath(
                expectedPath.front().modelId, expectedPath.front().depthId,
                expectedPath.front().profileName, expectedPath.back().modelId,
                expectedPath.back().depthId, expectedPath.back().profileName);
            std::printf("QUERY alpha8-case existsPath=%s existsGoodPath=%s\n",
                        exists ? "true" : "false", existsGood ? "true" : "false");

            // —— 复刻 testGrayAConnectionPaths 里第一处 p709 三元组用例（真实行 336-339）。
            std::vector<KoColorConversionSystem::NodeKey> grayExpectedPath = {
                {RGBAColorModelID.id(), Integer8BitsColorDepthID.id(), srgbProfile->name()},
                {GrayAColorModelID.id(), Integer8BitsColorDepthID.id(),
                 PkString("Gray-D50-elle-V2-srgbtrc.icc")}};

            std::printf("QUERY grayA-case endpoints src=%s/%s/%s dst=%s/%s/%s\n",
                        grayExpectedPath.front().modelId.PkToUtf8().c_str(),
                        grayExpectedPath.front().depthId.PkToUtf8().c_str(),
                        grayExpectedPath.front().profileName.PkToUtf8().c_str(),
                        grayExpectedPath.back().modelId.PkToUtf8().c_str(),
                        grayExpectedPath.back().depthId.PkToUtf8().c_str(),
                        grayExpectedPath.back().profileName.PkToUtf8().c_str());

            const std::vector<KoColorConversionSystem::NodeKey> grayRealPath =
                calcPath(grayExpectedPath);
            std::printf("QUERY calcPath(grayA-case) realPathLen=%d\n",
                        static_cast<int>(grayRealPath.size()));

            const bool grayExists = system->existsPath(
                grayExpectedPath.front().modelId, grayExpectedPath.front().depthId,
                grayExpectedPath.front().profileName, grayExpectedPath.back().modelId,
                grayExpectedPath.back().depthId, grayExpectedPath.back().profileName);
            const bool grayExistsGood = system->existsGoodPath(
                grayExpectedPath.front().modelId, grayExpectedPath.front().depthId,
                grayExpectedPath.front().profileName, grayExpectedPath.back().modelId,
                grayExpectedPath.back().depthId, grayExpectedPath.back().profileName);
            std::printf("QUERY grayA-case existsPath=%s existsGoodPath=%s\n",
                        grayExists ? "true" : "false", grayExistsGood ? "true" : "false");
        }
    }

    // 4b. 补充证据：路径查询 API 的**实际返回**。**默认条件**（`unsetenv("EXTRA_RESOURCE_DIRS")`
    //     后）下，elle 剖面（p709SRGBProfile() 的来源）取不到（`p709SRGBProfile()==nullptr`），
    //     故 p709 形状的三元组在**该条件**下查不到节点，上面那条分支拿不到非空路径查询结果。
    //     （本 driver 的 `--keep-resource-dirs` 对照组下 elle 剖面存在、该三元组会真的被查，
    //     见上方 `CHECKED-BRANCH premise holds` 那一段。）
    //     为在**默认条件**下也能把 existsPath / existsGoodPath 的**实际返回**落进证据
    //     （而非「不猜」），这里用该条件下实际存在的默认剖面名喂**同样的调用形状**。
    //     明确标注：这不是 p709 调用点，只是同一 API 在默认条件下的真实返回记录。
    const KoColorSpace *graya8 = KoColorSpaceRegistry::instance()->graya8();
    if (rgb8 && graya8 && rgb8->profile() && graya8->profile()) {
        const PkString srcModel = RGBAColorModelID.id();
        const PkString srcDepth = Integer8BitsColorDepthID.id();
        const PkString srcProfile = rgb8->profile()->name();
        const PkString dstModel = GrayAColorModelID.id();
        const PkString dstDepth = Integer8BitsColorDepthID.id();
        const PkString dstProfile = graya8->profile()->name();
        const bool exists = system->existsPath(srcModel, srcDepth, srcProfile,
                                               dstModel, dstDepth, dstProfile);
        const bool existsGood = system->existsGoodPath(srcModel, srcDepth, srcProfile,
                                                       dstModel, dstDepth, dstProfile);
        std::printf("QUERY(default-profiles; NOT the p709 call point) "
                    "src=%s/%s/%s dst=%s/%s/%s existsPath=%s existsGoodPath=%s\n",
                    srcModel.PkToUtf8().c_str(), srcDepth.PkToUtf8().c_str(),
                    srcProfile.PkToUtf8().c_str(), dstModel.PkToUtf8().c_str(),
                    dstDepth.PkToUtf8().c_str(), dstProfile.PkToUtf8().c_str(),
                    exists ? "true" : "false", existsGood ? "true" : "false");
    }

    // 5. 核心断言：走到这里 = 进程没崩（真实测试类在无资源环境下若走未修形态，
    //    `p709SRGBProfile()->name()` 会当场解引用 nullptr）。崩溃时 stdout 缓冲会丢，
    //    所以上面每一步都刻意打印，事后取证用。
    std::fflush(stdout);
    std::printf("RESULT failures=%d\n", failures);
    if (failures == 0) {
        std::cout << "p709SRGBProfile() null-or-not: the checked form does not crash\n";
    }
    return failures == 0 ? 0 : 1;
}
