/*
 *  SPDX-FileCopyrightText: 2012 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_input_manager_test.h"

#include <simpletest.h>

#include <PkInputEvent.h>
#include <PkSet.h>

#include "kis_single_action_shortcut.h"
#include "kis_stroke_shortcut.h"
#include "kis_abstract_input_action.h"
#include "kis_shortcut_matcher.h"


void KisInputManagerTest::testSingleActionShortcut()
{
    KisSingleActionShortcut s(0,0);
    s.setKey(PkSet<Pk::Key>{Pk::Key_Shift}, Pk::Key_Space);

    QVERIFY(s.match(PkSet<Pk::Key>{Pk::Key_Shift}, Pk::Key_Space));
    QVERIFY(!s.match(PkSet<Pk::Key>{Pk::Key_Control}, Pk::Key_Space));
    QVERIFY(!s.match(PkSet<Pk::Key>(), Pk::Key_Space));
    QVERIFY(!s.match(PkSet<Pk::Key>{Pk::Key_Shift}, Pk::Key_Escape));
    QVERIFY(!s.match(PkSet<Pk::Key>{Pk::Key_Shift}, KisSingleActionShortcut::WheelUp));

    s.setWheel(PkSet<Pk::Key>{Pk::Key_Shift}, KisSingleActionShortcut::WheelUp);

    QVERIFY(!s.match(PkSet<Pk::Key>{Pk::Key_Shift}, Pk::Key_Space));
    QVERIFY(!s.match(PkSet<Pk::Key>{Pk::Key_Control}, Pk::Key_Space));
    QVERIFY(!s.match(PkSet<Pk::Key>(), Pk::Key_Space));
    QVERIFY(!s.match(PkSet<Pk::Key>{Pk::Key_Shift}, Pk::Key_Escape));
    QVERIFY(s.match(PkSet<Pk::Key>{Pk::Key_Shift}, KisSingleActionShortcut::WheelUp));
}

void KisInputManagerTest::testStrokeShortcut()
{
    KisStrokeShortcut s(0,0);
    s.setButtons(PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                 PkSet<Pk::MouseButton>{Pk::LeftButton});

    QVERIFY(s.matchReady(PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                         PkSet<Pk::MouseButton>{Pk::LeftButton}));

    QVERIFY(s.matchReady(PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                         PkSet<Pk::MouseButton>()));

    QVERIFY(!s.matchReady(PkSet<Pk::Key>{Pk::Key_Control, Pk::Key_Alt},
                         PkSet<Pk::MouseButton>()));

    QVERIFY(!s.matchReady(PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                         PkSet<Pk::MouseButton>{Pk::RightButton}));

    QVERIFY(s.matchBegin(Pk::LeftButton));
    QVERIFY(!s.matchBegin(Pk::RightButton));
}

struct TestingAction : public KisAbstractInputAction
{
    TestingAction() : KisAbstractInputAction("TestingAction"), m_isHighResolution(false) { reset(); }
    ~TestingAction() {}

    void begin(int shortcut, PkInputEvent *event) override { m_beginIndex = shortcut; m_beginNonNull = event;}
    void end(PkInputEvent *event) override { m_ended = true; m_endNonNull = event; }
    void inputEvent(PkInputEvent * event) override { (void)event; m_gotInput = true; }

    void reset() {
        m_beginIndex = -1;
        m_ended = false;
        m_gotInput = false;
        m_beginNonNull = false;
        m_endNonNull = false;
    }

    bool supportsHiResInputEvents(int /*shortcut*/) const override {
        return m_isHighResolution;
    }

    void setHighResInputEvents(bool value) {
        m_isHighResolution = value;
    }

    int m_beginIndex;
    bool m_ended;
    bool m_gotInput;
    bool m_beginNonNull;
    bool m_endNonNull;

    bool m_isHighResolution;
};

KisSingleActionShortcut* createKeyShortcut(KisAbstractInputAction *action,
                                  int shortcutIndex,
                                  const PkSet<Pk::Key> &modifiers,
                                  Pk::Key key)
{
    KisSingleActionShortcut *s = new KisSingleActionShortcut(action, shortcutIndex);
    s->setKey(modifiers, key);
    return s;
}

KisStrokeShortcut* createStrokeShortcut(KisAbstractInputAction *action,
                                     int shortcutIndex,
                                     const PkSet<Pk::Key> &modifiers,
                                     Pk::MouseButton button)
{
    KisStrokeShortcut *s = new KisStrokeShortcut(action, shortcutIndex);
    s->setButtons(modifiers, PkSet<Pk::MouseButton>{button});
    return s;
}

void KisInputManagerTest::testKeyEvents()
{
    KisShortcutMatcher m;
    m.enterEvent();

    TestingAction *a = new TestingAction();


    m.addShortcut(
        createKeyShortcut(a, 10,
                          PkSet<Pk::Key>{Pk::Key_Shift},
                          Pk::Key_Enter));

    m.addShortcut(
        createKeyShortcut(a, 11,
                          PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                          Pk::Key_Enter));

    m.addShortcut(
        createStrokeShortcut(a, 12,
                             PkSet<Pk::Key>{Pk::Key_Shift},
                             Pk::RightButton));

    m.addShortcut(
        createStrokeShortcut(a, 13,
                             PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                             Pk::LeftButton));

    QCOMPARE(a->m_beginIndex, -1);

    // Test event with random values
    PkInputEvent mouseEvent(PkInputEvent::MouseMove,
                            PkPointF(), PkPointF(), PkPointF(),
                            Pk::LeftButton, Pk::NoButton, Pk::NoModifier);

    // Press Ctrl+Shift
    QVERIFY(!m.keyPressed(Pk::Key_Shift));
    QCOMPARE(a->m_beginIndex, -1);

    QVERIFY(!m.keyPressed(Pk::Key_Control));
    QCOMPARE(a->m_beginIndex, -1);


    // Complete Ctrl+Shift+Enter shortcut
    QVERIFY(m.keyPressed(Pk::Key_Enter));
    QCOMPARE(a->m_beginIndex, 11);
    QCOMPARE(a->m_ended, true);
    QCOMPARE(a->m_beginNonNull, false);
    QCOMPARE(a->m_endNonNull, false);
    a->reset();


    // Pressing mouse buttons is disabled since Enter is pressed
    QVERIFY(!m.buttonPressed(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);
    QVERIFY(!m.buttonReleased(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);


    // Release Enter, so the system should be ready for new shortcuts
    QVERIFY(!m.keyReleased(Pk::Key_Enter));
    QCOMPARE(a->m_beginIndex, -1);


    // Complete Ctrl+Shift+LB shortcut
    QVERIFY(m.buttonPressed(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, 13);
    QCOMPARE(a->m_ended, false);
    QCOMPARE(a->m_beginNonNull, true);
    QCOMPARE(a->m_endNonNull, false);
    a->reset();

    QVERIFY(m.buttonReleased(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);
    QCOMPARE(a->m_ended, true);
    QCOMPARE(a->m_beginNonNull, false);
    QCOMPARE(a->m_endNonNull, true);
    a->reset();


    // There is no Ctrl+Shift+RB shortcut
    QVERIFY(!m.buttonPressed(Pk::RightButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);

    QVERIFY(!m.buttonReleased(Pk::RightButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);


    // Check that Ctrl+Shift+Enter is still enabled
    QVERIFY(m.keyPressed(Pk::Key_Enter));
    QCOMPARE(a->m_beginIndex, 11);
    QCOMPARE(a->m_ended, true);
    QCOMPARE(a->m_beginNonNull, false);
    QCOMPARE(a->m_endNonNull, false);
    a->reset();

    // Check autorepeat
    QVERIFY(m.autoRepeatedKeyPressed(Pk::Key_Enter));
    QCOMPARE(a->m_beginIndex, 11);
    QCOMPARE(a->m_ended, true);
    QCOMPARE(a->m_beginNonNull, false);
    QCOMPARE(a->m_endNonNull, false);
    a->reset();

    QVERIFY(!m.keyReleased(Pk::Key_Enter));
    QCOMPARE(a->m_beginIndex, -1);


    // Release Ctrl
    QVERIFY(!m.keyReleased(Pk::Key_Control));
    QCOMPARE(a->m_beginIndex, -1);


    // There is no Shift+LB shortcut
    QVERIFY(!m.buttonPressed(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);

    QVERIFY(!m.buttonReleased(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);


    // But there *is* Shift+RB shortcut
    QVERIFY(m.buttonPressed(Pk::RightButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, 12);
    QCOMPARE(a->m_ended, false);
    QCOMPARE(a->m_beginNonNull, true);
    QCOMPARE(a->m_endNonNull, false);
    a->reset();

    QVERIFY(m.buttonReleased(Pk::RightButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);
    QCOMPARE(a->m_ended, true);
    QCOMPARE(a->m_beginNonNull, false);
    QCOMPARE(a->m_endNonNull, true);
    a->reset();


    // Check that Shift+Enter still works
    QVERIFY(m.keyPressed(Pk::Key_Enter));
    QCOMPARE(a->m_beginIndex, 10);
    QCOMPARE(a->m_ended, true);
    QCOMPARE(a->m_beginNonNull, false);
    QCOMPARE(a->m_endNonNull, false);
    a->reset();

    m.leaveEvent();
}

void KisInputManagerTest::testReleaseUnnecessaryModifiers()
{
    KisShortcutMatcher m;
    m.enterEvent();

    TestingAction *a = new TestingAction();

    m.addShortcut(
        createStrokeShortcut(a, 13,
                             PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                             Pk::LeftButton));

    // Test event with random values
    PkInputEvent mouseEvent(PkInputEvent::MouseMove,
                            PkPointF(), PkPointF(), PkPointF(),
                            Pk::LeftButton, Pk::NoButton, Pk::NoModifier);

    // Press Ctrl+Shift
    QVERIFY(!m.keyPressed(Pk::Key_Shift));
    QCOMPARE(a->m_beginIndex, -1);

    QVERIFY(!m.keyPressed(Pk::Key_Control));
    QCOMPARE(a->m_beginIndex, -1);

    // Complete Ctrl+Shift+LB shortcut
    QVERIFY(m.buttonPressed(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, 13);
    QCOMPARE(a->m_ended, false);
    a->reset();

    // Release Ctrl
    QVERIFY(!m.keyReleased(Pk::Key_Control));
    QCOMPARE(a->m_beginIndex, -1);
    QCOMPARE(a->m_ended, false);

    // Release Shift
    QVERIFY(!m.keyReleased(Pk::Key_Shift));
    QCOMPARE(a->m_beginIndex, -1);
    QCOMPARE(a->m_ended, false);

    // Release LB, now it should end
    QVERIFY(m.buttonReleased(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);
    QCOMPARE(a->m_ended, true);
    a->reset();

    m.leaveEvent();
}

void KisInputManagerTest::testMouseMoves()
{
    KisShortcutMatcher m;
    m.enterEvent();

    TestingAction *a = new TestingAction();

    m.addShortcut(
        createStrokeShortcut(a, 13,
                             PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                             Pk::LeftButton));

    // Test event with random values
    PkInputEvent mouseEvent(PkInputEvent::MouseMove,
                            PkPointF(), PkPointF(), PkPointF(),
                            Pk::LeftButton, Pk::NoButton, Pk::NoModifier);


    // Press Ctrl+Shift
    QVERIFY(!m.keyPressed(Pk::Key_Shift));
    QCOMPARE(a->m_beginIndex, -1);

    QVERIFY(!m.keyPressed(Pk::Key_Control));
    QCOMPARE(a->m_beginIndex, -1);

    QVERIFY(!m.pointerMoved(&mouseEvent));
    QCOMPARE(a->m_gotInput, false);

    // Complete Ctrl+Shift+LB shortcut
    QVERIFY(m.buttonPressed(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, 13);
    QCOMPARE(a->m_ended, false);
    QCOMPARE(a->m_gotInput, false);
    a->reset();

    QVERIFY(m.pointerMoved(&mouseEvent));
    QCOMPARE(a->m_gotInput, true);
    a->reset();

    // Release Ctrl
    QVERIFY(!m.keyReleased(Pk::Key_Control));
    QCOMPARE(a->m_beginIndex, -1);
    QCOMPARE(a->m_ended, false);
    QCOMPARE(a->m_gotInput, false);

    // Release Shift
    QVERIFY(!m.keyReleased(Pk::Key_Shift));
    QCOMPARE(a->m_beginIndex, -1);
    QCOMPARE(a->m_ended, false);

    // Release LB, now it should end
    QVERIFY(m.buttonReleased(Pk::LeftButton, &mouseEvent));
    QCOMPARE(a->m_beginIndex, -1);
    QCOMPARE(a->m_ended, true);
    a->reset();

    m.leaveEvent();
}

#include "kis_incremental_average.h"

void KisInputManagerTest::testIncrementalAverage()
{
    KisIncrementalAverage avg(3);

    QCOMPARE(avg.pushThrough(10), 10);
    QCOMPARE(avg.pushThrough(20), 13);
    QCOMPARE(avg.pushThrough(30), 20);
    QCOMPARE(avg.pushThrough(30), 26);
    QCOMPARE(avg.pushThrough(30), 30);

}

SIMPLE_TEST_MAIN(KisInputManagerTest)
