# D-23 的机械改名规则，唯一允许对副本做的改动（源树本身不碰）。
#
# 顺序有讲究：QVERIFY2 必须排在 QVERIFY 前（否则 QVERIFY 的规则先把
# QVERIFY2( 错吃成 PK_VERIFY2(... 不对，是吃成 PK_VERIFY2( 里多一个左括号
# 之前就已经匹配掉 QVERIFY(，留下多余的 "2("）；QTEST_APPLESS_MAIN /
# QTEST_GUILESS_MAIN 必须排在 QTEST_MAIN 前，否则 QTEST_MAIN 的规则会先
# 匹配到它们的前缀。\b 在 GNU sed 下管用，但各条模式后面已经跟着硬边界
# （左括号或非标识符字符），两层保险都留着。
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
s/\bQTest::addRow/PkTest::addRow/g
s/\bQTest::newRow/PkTest::newRow/g
s/\bQTest::qExec/PkTest::qExec/g
s/\bqFuzzyCompare(/pkFuzzyCompare(/g
