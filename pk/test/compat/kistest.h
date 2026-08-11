#pragma once
#include "QObject"
#include "QTest"

// sdk/tests/kistest.h 的最小转发。真品按链接的资源库（pigment/flake/image/...）
// 选不同的 KISTEST_MAIN 展开，还要接资源目录、QApplication、日志分类过滤——
// 那一整套归 S0（真正剥资源系统时）处理。R-11 只给一个能让"用了 KISTEST_MAIN
// 但不碰资源"的测试编过、跑对的最小垫片。
#define KISTEST_MAIN(TestObject) PK_TEST_MAIN(TestObject)
