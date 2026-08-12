# D-23 的机械改名规则，唯一允许对副本做的改动（源树本身不碰）。
# 规则集与 pk/test/README.md §1 的实现表一一对应：那张表加一项，
# 这里就要加一条，否则 S-00 拿这份表跑全量 sed 会漏掉它。
#
# 顺序有讲究：每条模式后面都跟着硬边界（左括号，或 `::` 这种非标识符字符），
# 所以一个规则的匹配文本不会是另一个规则匹配文本的前缀——`QVERIFY(` 不会命中
# `QVERIFY2(`（后面紧跟的是 `2` 不是 `(`），`QTEST_MAIN(` 也不会命中
# `QTEST_APPLESS_MAIN(`/`QTEST_GUILESS_MAIN(` 中间那段。因此这份列表本身对
# 顺序不敏感；仍然保留人工阅读顺序（先宏后 `QTest::` 成员）只是为了好读，
# 不是正确性要求。
s/\bQVERIFY2(/PK_VERIFY2(/g
s/\bQCOMPARE(/PK_COMPARE(/g
s/\bQVERIFY(/PK_VERIFY(/g
s/\bQFETCH(/PK_FETCH(/g
s/\bQFAIL(/PK_FAIL(/g
s/\bQSKIP(/PK_SKIP(/g
s/\bQEXPECT_FAIL(/PK_EXPECT_FAIL(/g
s/\bQTEST_APPLESS_MAIN(/PK_TEST_APPLESS_MAIN(/g
s/\bQTEST_GUILESS_MAIN(/PK_TEST_GUILESS_MAIN(/g
s/\bQTEST_MAIN(/PK_TEST_MAIN(/g
s/\bQTest::addColumn/PkTest::addColumn/g
s/\bQTest::currentDataTag/PkTest::currentDataTag/g
s/\bQTest::addRow/PkTest::addRow/g
s/\bQTest::newRow/PkTest::newRow/g
s/\bQTest::qExec/PkTest::qExec/g
s/\bQTest::qFail/PkTest::qFail/g
s/\bqFuzzyCompare(/pkFuzzyCompare(/g

# QTest::qCompare：真实调用点存在（sdk/tests/testutil.h 的 KIS_COMPARE_FLT
# 宏），但 R-11 判定排除（见 README.md §2）——PkTest::qCompare 没有实现。
# 这条规则仍然保留：sed 跑完之后调用点会变成 `PkTest::qCompare`，在
# PK_TEST_NO_QT_MACRO_ALIASES 下编译期报"没有这个成员"，比留一个改不掉的
# `QTest::qCompare` 更早暴露问题——不留这条规则会让这处调用点在全量 sed 之后
# 仍然带着 `QTest::` 前缀留在源码里，跟"S-00 之后源码里不再出现 QTest" 这条
# 目标不符。
s/\bQTest::qCompare/PkTest::qCompare/g
