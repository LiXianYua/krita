#pragma once
#include "QObject"
#include "QTest"

// sdk/tests/kistest.h 的最小转发。真品按链接的资源库（pigment/flake/image/...）
// 选不同的 KISTEST_MAIN 展开，还要接资源目录、QApplication、日志分类过滤——
// 那一整套归 S0（真正剥资源系统时）处理。R-11 只给一个能让"用了 KISTEST_MAIN
// 但不碰资源"的测试编过、跑对的最小垫片。
//
// R-77 订正：上面「归 S0 处理」的归因已失效。真品的 pk 分支**已在
// sdk/tests/kistest.h 落地**（KRITA_TESTSDK_PK_NATIVE → SIMPLE_TEST_MAIN），
// 需要的不是本文件。资源目录的 qputenv 也**不是缺**——D-12 之后插件层是静态注册、
// 没有可加载的插件 DSO（S-09-g 落地），KRITA_PLUGIN_PATH 在任何栈上都不再生效；
// 需要注册表条目的测试走静态链入 provider（plugins/impex/libkra/tests/CMakeLists.txt:22,26
// 的 -force_load、聚合器 plugins/kritaplugin_registry/），锁外承接方
// libs/image/tests/CMakeLists.txt。
//
// **本文件今日已死（判死不删）**：kritatestsdk_pk 生成 wrap TU 时由
// sdk/tests/pk_test_support.cmake 把 sdk/tests 显式排在 include 表最前，于是
// `<kistest.h>` 与 `"kistest.h"`（testui.h 用引号，先查自己所在的 sdk/tests/）都先命中
// 真品 sdk/tests/kistest.h，永远轮不到本转发头。唯一会命中它的条件：某消费者在
// **没有** sdk/tests 在前的 include 表下 `#include <kistest.h>`——今日树里不存在这样的
// target。留着是为了让误改真品的人看见这条路已废。
#define KISTEST_MAIN(TestObject) PK_TEST_MAIN(TestObject)
