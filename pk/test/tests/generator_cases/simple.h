#pragma once
#include <QObject>

class NotATest
{
    void notDiscovered();
};

class SimpleTest : public QObject
{
    Q_OBJECT
public:
    int helper() { return 0; }
private Q_SLOTS:
    void initTestCase();
    void initTestCase_data();
    void testAlpha();      // 行尾注释
    void testBeta();
    void testBeta_data();
    /* void commentedOutBlock(); */
    // void commentedOutLine();
    void cleanup();
private:
    void notATestFunction();
};

class NestedBracesTest : public QObject
{
    Q_OBJECT
    struct Inner { int x; };
private Q_SLOTS:
    void testGamma();
};

// 裸 Q_SIGNALS:（不带 public/protected/private 前缀）是 Krita 真实代码里
// 常见的写法——_ACCESS 必须把它当块边界，否则信号会被静默收进 functions()。
class SignalBoundaryTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void realTest();
Q_SIGNALS:
    void changed();
private:
    void notATest();
};
