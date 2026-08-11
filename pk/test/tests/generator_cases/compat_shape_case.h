#pragma once

// Task 6 的 compat 自测输入头：走真实的 compat/QObject 垫片（-I pk/test/compat 下
// `#include <QObject>` 解析到我们的实现），不像 self_assert_case.h 那样本地
// #define 等价展开。证明 compat 垫片本身、而不只是生成器认识的 token 形状，
// 与 pk_test_moc.py 的扫描规则严丝合缝。
#include <QObject>

class CompatShapeCase : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testPasses();
    void testFails();
};
