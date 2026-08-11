#pragma once
#include "QObject"
#include "QTest"

// sdk/tests/simpletest.h 的零 Qt 替身。真品做的六件事里五件都是 D-30 明写要删的
// （QLocale::setDefault / QStandardPaths::setTestModeEnabled / QApplication /
//  AA_Use96Dpi / QTEST_DISABLE_KEYPAD_NAVIGATION），只有最后一行 qExec 是真依赖。
//
// 资源目录那两个 qputenv（EXTRA_RESOURCE_DIRS / KRITA_PLUGIN_PATH）归 S0 处理，
// R-11 的两个试接目标都不碰资源。
#define SIMPLE_MAIN_IMPL(TestObject)                    \
    TestObject tc;                                      \
    return PkTest::qExec(&tc, argc, argv);

#define SIMPLE_TEST_MAIN(TestObject)                    \
    int main(int argc, char *argv[])                    \
    {                                                   \
        SIMPLE_MAIN_IMPL(TestObject)                    \
    }
