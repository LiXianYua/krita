/*
 *  SPDX-FileCopyrightText: 2019 Tusooa Zhu <tusooa@vista.aero>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisSignalAutoConnectionTest.h"

#include <kis_signal_auto_connection.h>

namespace {
constexpr PkConnectionType autoConnection =
    static_cast<PkConnectionType>(Qt::AutoConnection);
}

void KisSignalAutoConnectionTest::testMacroConnection()
{
    QScopedPointer<TestClass> test1(new TestClass());
    QScopedPointer<TestClass> test2(new TestClass());
    KisSignalAutoConnectionsStore conn;
    conn.addConnection(test1.data(), &TestClass::sigTest1,
                       test2.data(), &TestClass::slotTest1, autoConnection);
    Q_EMIT test1->sigTest1();
    QVERIFY(test2->m_test1Called);
    test2->m_test1Called = false;
    conn.clear();
    Q_EMIT test1->sigTest1();
    QVERIFY(test2->m_test1Called == false);
}

void KisSignalAutoConnectionTest::testMemberFunctionConnection()
{
    QScopedPointer<TestClass> test1(new TestClass());
    QScopedPointer<TestClass> test2(new TestClass());
    KisSignalAutoConnectionsStore conn;
    conn.addConnection(test1.data(), &TestClass::sigTest1, test2.data(), &TestClass::slotTest1,
                       autoConnection);
    Q_EMIT test1->sigTest1();
    QVERIFY(test2->m_test1Called);
    test2->m_test1Called = false;
    conn.clear();
    Q_EMIT test1->sigTest1();
    QVERIFY(test2->m_test1Called == false);
}

void KisSignalAutoConnectionTest::testOverloadConnection()
{
    QScopedPointer<TestClass> test1(new TestClass());
    QScopedPointer<TestClass> test2(new TestClass());
    KisSignalAutoConnectionsStore conn;
    conn.addConnection(test1.data(), QOverload<const QString &, const QString &>::of(&TestClass::sigTest2),
                       test2.data(), QOverload<const QString &, const QString &>::of(&TestClass::slotTest2), autoConnection);
    conn.addConnection(test1.data(), QOverload<int>::of(&TestClass::sigTest2),
                       test2.data(), QOverload<int>::of(&TestClass::slotTest2), autoConnection);
    Q_EMIT test1->sigTest2("foo", "bar");
    QVERIFY(test2->m_str1 == "foo");
    QVERIFY(test2->m_str2 == "bar");
    Q_EMIT test1->sigTest2(5);
    QVERIFY(test2->m_number == 5);
    conn.clear();
    Q_EMIT test1->sigTest2("1", "2");
    QVERIFY(test2->m_str1 == "foo");
    QVERIFY(test2->m_str2 == "bar");
    conn.addConnection(test1.data(), QOverload<const QString &, const QString &>::of(&TestClass::sigTest2),
                       test2.data(), static_cast<void (TestClass::*)(const QString &)>(&TestClass::slotTest2), autoConnection);
    Q_EMIT test1->sigTest2("3", "4");
    QVERIFY(test2->m_str1 == "3");
    QVERIFY(test2->m_str2 == "");
}

void KisSignalAutoConnectionTest::testSignalToSignalConnection()
{
    QScopedPointer<TestClass> test1(new TestClass());
    QScopedPointer<TestClass> test2(new TestClass());
    KisSignalAutoConnectionsStore conn;
    conn.addConnection(test1.data(), QOverload<int>::of(&TestClass::sigTest2),
                       test2.data(), QOverload<int>::of(&TestClass::sigTest2), autoConnection);
    conn.addConnection(test2.data(), QOverload<int>::of(&TestClass::sigTest2),
                       test2.data(), QOverload<int>::of(&TestClass::slotTest2), autoConnection);
    Q_EMIT test1->sigTest2(10);
    QVERIFY(test2->m_number == 10);
    conn.clear();
    conn.addConnection(test1.data(), QOverload<int>::of(&TestClass::sigTest2),
                       test2.data(), QOverload<int>::of(&TestClass::sigTest2), autoConnection);
    conn.addConnection(test2.data(), QOverload<int>::of(&TestClass::sigTest2),
                       test2.data(), QOverload<int>::of(&TestClass::slotTest2), autoConnection);
    Q_EMIT test1->sigTest2(50);
    QVERIFY(test2->m_number == 50);
}

void KisSignalAutoConnectionTest::testDestroyedObject()
{
    QScopedPointer<TestClass> test1(new TestClass());
    QScopedPointer<TestClass> test2(new TestClass());
    KisSignalAutoConnectionsStore conn;
    conn.addConnection(test1.data(), QOverload<int>::of(&TestClass::sigTest2),
                       test2.data(), QOverload<int>::of(&TestClass::slotTest2), autoConnection);
    Q_EMIT test1->sigTest2(10);
    QVERIFY(test2->m_number == 10);
    test2.reset(0);
    conn.clear();
}

void TestClass::sigTest1()
{
    activateSignal(this, PkMemberFnKey::from(&TestClass::sigTest1));
}

void TestClass::sigTest2(const QString &arg1, const QString &arg2)
{
    activateSignal<const QString &, const QString &>(this,
        PkMemberFnKey::from(static_cast<void (TestClass::*)(const QString &, const QString &)>(&TestClass::sigTest2)),
        arg1, arg2);
}

void TestClass::sigTest2(int arg)
{
    activateSignal(this, PkMemberFnKey::from(static_cast<void (TestClass::*)(int)>(&TestClass::sigTest2)), arg);
}

TestClass::TestClass(PkObject *parent)
    : PkObject(parent)
    , m_test1Called(false)
    , m_str1()
    , m_str2()
    , m_number(0)
{
}

TestClass::~TestClass()
{
}

void TestClass::slotTest1()
{
    m_test1Called = true;
}

void TestClass::slotTest2(const QString &arg1, const QString &arg2)
{
    m_str1 = arg1;
    m_str2 = arg2;
}

void TestClass::slotTest2(const QString &arg)
{
    m_str1 = arg;
    m_str2 = QString();
}

void TestClass::slotTest2(int arg)
{
    m_number = arg;
}

SIMPLE_TEST_MAIN(KisSignalAutoConnectionTest)
