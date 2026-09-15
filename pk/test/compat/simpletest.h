#pragma once
#include "QObject"
#include "QTest"

// sdk/tests/simpletest.h 的零 Qt 替身。真品做的六件事里五件都是 D-30 明写要删的
// （QLocale::setDefault / QStandardPaths::setTestModeEnabled / QApplication /
//  AA_Use96Dpi / QTEST_DISABLE_KEYPAD_NAVIGATION），只有最后一行 qExec 是真依赖。
//
// 资源目录的两个 qputenv（EXTRA_RESOURCE_DIRS / KRITA_PLUGIN_PATH）**不是漏了**。
// R-77 订正：原文「归 S0 处理」已失效——S-00/S-06 那条线已 VERIFIED，这条归因今天
// 无人承接。现状：
//   * KRITA_PLUGIN_PATH —— 决策 D-12 之后插件层是静态注册、没有可加载的插件 DSO
//     （plugins/** 里 0 个 MODULE 目标，已由 S-09-g 落地），该环境变量在任何栈上都不再生效。
//   * EXTRA_RESOURCE_DIRS —— pk 栈上没有任何测试读资源目录（R-11 的两个试接目标都不碰资源）。
// 真需要注册表条目的测试改走既成的「静态链入 provider」范式，锁外承接方是
// libs/image/tests/CMakeLists.txt（范式样例见 plugins/impex/libkra/tests/CMakeLists.txt:22,26
// 的 -force_load 与 plugins/kritaplugin_registry/ 聚合器）。
// 同一条说明的真源在 sdk/tests/simpletest.h 的 KRITA_SIMPLE_TEST_PLUGIN_PATH_SETUP 处。
#define SIMPLE_MAIN_IMPL(TestObject)                    \
    TestObject tc;                                      \
    return PkTest::qExec(&tc, argc, argv);

#define SIMPLE_TEST_MAIN(TestObject)                    \
    int main(int argc, char *argv[])                    \
    {                                                   \
        SIMPLE_MAIN_IMPL(TestObject)                    \
    }
