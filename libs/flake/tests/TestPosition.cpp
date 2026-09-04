/*
 *  SPDX-FileCopyrightText: 2006-2010 Thomas Zander <zander@kde.org>
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestPosition.h"

#include <MockShapes.h>
#include <PkPoint.h>
#include <simpletest.h>

TestPosition::TestPosition()
        : shape1(0),
        shape2(0),
        childShape1(0),
        childShape2(0),
        container(0),
        container2(0)
{
}

void TestPosition::init()
{
    shape1 = new MockShape();
    shape1->setPosition(PkPointF(50, 50));
    shape1->setSize(PkSize(50, 50));
    shape2 = new MockShape();
    shape2->setPosition(PkPointF(20, 20));
    shape2->setSize(PkSize(50, 50));

    childShape1 = new MockShape();
    childShape1->setPosition(PkPointF(20, 20));
    childShape1->setSize(PkSize(50, 50));
    container = new MockContainer();
    container->setPosition(PkPointF(100, 100));
    container->addShape(childShape1);
    container->setInheritsTransform(childShape1, false);

    childShape2 = new MockShape();
    childShape2->setPosition(PkPointF(25, 25));
    childShape2->setSize(PkSizeF(10, 15));
    container2 = new MockContainer();
    container2->setPosition(PkPointF(100, 200));
    container2->setSize(PkSizeF(100, 100));
    container2->rotate(90);
    container2->addShape(childShape2);
}

void TestPosition::cleanup()
{
    delete container;
    delete container2;
    delete shape1;
    delete shape2;
}

void TestPosition::testBasePosition()
{
    // internal consistency tests.
    QCOMPARE(shape1->position(), PkPointF(50, 50));
    QCOMPARE(shape2->position(), PkPointF(20, 20));
    QCOMPARE(childShape1->position(), PkPointF(20, 20));
    QCOMPARE(container->position(), PkPointF(100, 100));
}

void TestPosition::testAbsolutePosition()
{
    QCOMPARE(shape1->absolutePosition(), PkPointF(75, 75));
    QCOMPARE(shape2->absolutePosition(), PkPointF(45, 45));

    // translated
    QCOMPARE(childShape1->absolutePosition(), PkPointF(100 + 20 + 25, 100 + 20 + 25));

    // rotated
    container2->setInheritsTransform(childShape2, false);
    QCOMPARE(container2->absolutePosition(), PkPointF(150, 250));
    QCOMPARE(childShape2->absolutePosition(), PkPointF(130, 232.5));
    container2->setInheritsTransform(childShape2, true);
    QCOMPARE(childShape2->absolutePosition(), PkPointF(167.5, 230));

    shape1->rotate(90);
    shape1->setPosition(PkPointF(10, 10));

    QCOMPARE(shape1->absolutePosition(), PkPointF(10 + 25, 10 + 25));
    QCOMPARE(shape1->absolutePosition(KoFlake::Center), PkPointF(10 + 25, 10 + 25));
    QCOMPARE(shape1->absolutePosition(KoFlake::TopLeft), PkPointF(10 + 50, 10));
    QCOMPARE(shape1->absolutePosition(KoFlake::BottomRight), PkPointF(10, 10 + 50));

    QCOMPARE(container2->absolutePosition(KoFlake::TopLeft), PkPointF(200, 200));
}

void TestPosition::testSetAbsolutePosition()
{
    shape1->rotate(-90); // rotate back as we are rotating relative
    shape1->setPosition(PkPointF(10, 10));
    QCOMPARE(shape1->absolutePosition(), PkPointF(10 + 25, 10 + 25));
    shape1->setAbsolutePosition(PkPointF(10, 10));
    QCOMPARE(shape1->absolutePosition(), PkPointF(10, 10));
    shape1->rotate(45);
    QCOMPARE(shape1->absolutePosition(), PkPointF(10, 10));

    childShape1->setAbsolutePosition(PkPointF(0, 0));
    QCOMPARE(childShape1->position(), PkPointF(-125, -125));
    QCOMPARE(childShape1->absolutePosition(), PkPointF(0, 0));

    QCOMPARE(container2->position(), PkPointF(100, 200));  // make sure nobody changed it
    container2->setInheritsTransform(childShape2, false);
    childShape2->setAbsolutePosition(PkPointF(0, 0));
    QCOMPARE(childShape2->position(), PkPointF(-100 - 5, -200 - 7.5));
    QCOMPARE(childShape2->absolutePosition(), PkPointF(0, 0));

    container2->setInheritsTransform(childShape2, true);
    childShape2->setAbsolutePosition(PkPointF(0, 0));
    QCOMPARE(childShape2->absolutePosition(), PkPointF(0, 0));
    QCOMPARE(childShape2->position(), PkPointF(-200 - 5, 200 - 7.5));
}

void TestPosition::testSetAbsolutePosition2()
{
    shape1->rotate(90);
    shape1->setAbsolutePosition(PkPointF(100, 100));
    QCOMPARE(shape1->absolutePosition(), PkPointF(100, 100));

    shape1->setAbsolutePosition(PkPointF(100, 100), KoFlake::TopLeft);
    QCOMPARE(shape1->absolutePosition(KoFlake::TopLeft), PkPointF(100, 100));

    childShape1->setAbsolutePosition(PkPointF(0, 0), KoFlake::BottomRight);
    QCOMPARE(childShape1->position(), PkPointF(-150, -150));

    childShape1->setAbsolutePosition(PkPointF(0, 0), KoFlake::BottomLeft);
    QCOMPARE(childShape1->position(), PkPointF(-100, -150));

    childShape1->setAbsolutePosition(PkPointF(0, 0), KoFlake::TopRight);
    QCOMPARE(childShape1->position(), PkPointF(-150, -100));

    container2->setInheritsTransform(childShape2, true);
    childShape2->setAbsolutePosition(PkPointF(0, 0), KoFlake::TopLeft);
    QCOMPARE(childShape2->position(), PkPointF(-200, 200));
}

void TestPosition::testSetAndGetRotation()
{
    shape1->rotate(180);
    QCOMPARE(shape1->rotation(), 180.0);
    shape1->rotate(2);
    QCOMPARE(shape1->rotation(), 182.0);
    shape1->rotate(4);
    QCOMPARE(shape1->rotation(), 186.0);
    shape1->rotate(358);
    QCOMPARE(shape1->rotation(), 184.0);
}

QTEST_GUILESS_MAIN(TestPosition)
