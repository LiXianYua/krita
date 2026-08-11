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
    void testAlpha();      // 行尾注释
    void testBeta();
    void testBeta_data();
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
