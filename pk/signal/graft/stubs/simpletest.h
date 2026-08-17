#pragma once
// 试接脚手架 —— 不是 R-05 的交付物。
//
// 覆盖 pk/test/compat/simpletest.h：原版用引号 `#include "QObject"`，引号 include
// 的「先查当前文件所在目录」规则让它命中同目录的 pk/test/compat/QObject
// （QObject=PkTestObject、Q_SIGNALS=空、signals=空）。本试接的测试类
// KisSignalAutoConnectionTest/TestClass 带 Q_SIGNALS 信号段与成员函数指针连接，
// 需要 QObject=PkObject、Q_SIGNALS=public（信号要 public 才能被外部取地址，
// 裸 `Q_SIGNALS:` 展开成裸冒号也不合法）——那只有 pk/signal/compat/QObject 给得出。
//
// 这里把 QObject 的 include 改成尖括号：尖括号按 -I 顺序找，而 graft_run.sh
// 的 -I 里 pk/signal/compat 排在 pk/test/compat 之前，于是 <QObject> 命中
// pk/signal 版。QTest 仍指向 pk/test（唯一提供者）。QString 显式加一条尖括号
// include（→ pk/string/compat/QString）：真实 sdk/tests/simpletest.h 经 QTest/
// QLocale 传递提供了 QString，本测试头只 include 了 <simpletest.h> 却声明
// QString 成员，必须复刻这条传递性。宏体照抄原版，一字不改。
#include <QObject>
#include <QString>
#include <QTest>

#define SIMPLE_MAIN_IMPL(TestObject)                    \
    TestObject tc;                                      \
    return PkTest::qExec(&tc, argc, argv);

#define SIMPLE_TEST_MAIN(TestObject)                    \
    int main(int argc, char *argv[])                    \
    {                                                   \
        SIMPLE_MAIN_IMPL(TestObject)                    \
    }
