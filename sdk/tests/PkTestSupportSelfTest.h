#pragma once

// R-65 机器自测：证明 sdk/tests 交付的 pk 测试栈端到端可用
// （kritatestsdk_pk + pk_add_test + wrap TU + PkTestCompatAll.h 预激活）。
//
// 类形态刻意与 libs/**/tests 里的真实 Krita 测试类逐条一致——`SIMPLE_TEST_MAIN`
// 入口、`public QObject` 基类、`Q_OBJECT`、`private Q_SLOTS:` 段里的测试函数。
// 若这里改用 `PK_TEST_MAIN` 或省掉 Q_OBJECT，证明的就不是真实测试面走的那条路。
//
// 本文件**不是**被测对象，是机器的自测夹具；它零 Krita 库依赖，所以它的红/绿只
// 反映「机器」，不反映「测试面缺垫片」——两者混在一起时这一点很重要。
#include <simpletest.h>

class PkTestSupportSelfTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCompatMacroAliases();
    void testForceIncludedScalarsAreActive();
    void testPreactivatedCompatTypesAreActive();

    // R-77（brief §3.4）：把 sdk/tests/filestest.h 端口化出来的「只依赖垫片面」的
    // 三个函数直接拉到**收尾路径**上真跑一遍——真造文件、真改权限、真删、断言可
    // 观察结果。选这个 target 的理由是它已经在 ctest 里跑得到（`.superpowers/sdd/`
    // 下的脚本不算数）。
    void testFilestestPortedHelpersAreRunnable();
    // R-77：QTemporaryFile 垫片在 pk 栈里**没有**可运行的消费者（唯一调用点
    // plugins/impex/exr/tests/kis_exr_test.cpp 走 Qt 栈），所以在这里钉住它
    // 探针实测的三条语义（open 前 fileName 为空 / close 不删 / 析构按 autoRemove 删）。
    void testTemporaryFileShimCreatesAndAutoremoves();
    // R-77：把 QDir::cleanPath 与 QFileInfo::absoluteFilePath 共用那份算法的**探针
    // 契约**（真 Qt 5.15.7 实测的 18 + 8 条字符串）逐条钉死——端到端只走得到其中
    // 一两条形态，剩下的只能靠这张表。
    void testPathCleanProbeContract();
};
