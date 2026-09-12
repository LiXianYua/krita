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
};
