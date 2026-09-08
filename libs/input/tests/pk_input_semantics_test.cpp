/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkInputEvent.h>
#include <PkKeySequence.h>
#include <PkSet.h>

#include "kis_abstract_input_action.h"
#include "kis_shortcut_matcher.h"
#include "kis_native_gesture_shortcut.h"
#include "kis_single_action_shortcut.h"
#include "kis_stroke_shortcut.h"
#include "kis_touch_shortcut.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool closeEnough(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < 1.0e-12;
}

void testEventPayloadsMatchQt515Oracle()
{
    const PkInputEvent mouse(PkInputEvent::MouseButtonRelease,
                             PkPointF(12.25, -3.5),
                             PkPointF(22.5, 7.75),
                             PkPointF(102.75, 88.125),
                             Pk::LeftButton,
                             Pk::NoButton,
                             Pk::ShiftModifier | Pk::AltModifier,
                             1234);

    require(static_cast<int>(mouse.type()) == 3, "mouse release type must match Qt 5.15");
    require(closeEnough(mouse.localPosition().x(), 12.25) && closeEnough(mouse.localPosition().y(), -3.5),
            "mouse local position must be preserved");
    require(closeEnough(mouse.windowPosition().x(), 22.5) && closeEnough(mouse.windowPosition().y(), 7.75),
            "mouse window position must be preserved");
    require(closeEnough(mouse.globalPosition().x(), 102.75) && closeEnough(mouse.globalPosition().y(), 88.125),
            "mouse global position must be preserved");
    require(mouse.button() == Pk::LeftButton && mouse.buttons() == Pk::NoButton,
            "mouse button and held-button state must remain distinct");
    require(mouse.modifiers() == (Pk::ShiftModifier | Pk::AltModifier),
            "mouse modifiers must be preserved");
    require(mouse.timestamp() == 1234, "mouse timestamp must be preserved");

    const PkTabletEvent tablet(PkInputEvent::TabletMove,
                               PkPointF(6.5, -2.25),
                               PkPointF(106.5, 97.75),
                               Pk::RightButton,
                               Pk::RightButton | Pk::MiddleButton,
                               Pk::MetaModifier,
                               0.75,
                               -12,
                               34,
                               -0.125,
                               123.5,
                               7,
                               998877);
    require(static_cast<int>(tablet.type()) == 87,
            "tablet move type must match Qt 5.15");
    require(closeEnough(tablet.localPosition().x(), 6.5)
                && closeEnough(tablet.globalPosition().x(), 106.5),
            "tablet local and global positions must be preserved");
    require(tablet.button() == Pk::RightButton
                && tablet.buttons() == (Pk::RightButton | Pk::MiddleButton)
                && tablet.modifiers() == Pk::MetaModifier,
            "tablet button, held-button state, and modifiers must be preserved");
    require(closeEnough(tablet.pressure(), 0.75)
                && tablet.xTilt() == -12
                && tablet.yTilt() == 34
                && closeEnough(tablet.tangentialPressure(), -0.125)
                && closeEnough(tablet.rotation(), 123.5)
                && tablet.z() == 7
                && tablet.uniqueId() == 998877,
            "tablet axis and identity payload must match Qt 5.15");

    PkTouchPoint point(17);
    point.setState(Pk::TouchPointMoved);
    point.setPosition(PkPointF(9.5, -4.25));
    point.setStartPosition(PkPointF(1.25, 2.5));
    point.setGlobalPosition(PkPointF(109.5, 95.75));
    point.setPressure(0.625);

    PkList<PkTouchPoint> points{point};
    const PkTouchEvent touch(PkInputEvent::TouchUpdate,
                             Pk::ControlModifier,
                             Pk::TouchPointMoved,
                             points,
                             4321);
    points[0].setPosition(PkPointF(999.0, 999.0));

    require(static_cast<int>(touch.type()) == 195, "touch update type must match Qt 5.15");
    require(touch.touchPoints().size() == 1 && touch.touchPoints().at(0).id() == 17,
            "touch point identity must be preserved");
    require(touch.touchPoints().at(0).state() == Pk::TouchPointMoved,
            "touch point state must be preserved");
    require(static_cast<int>(touch.touchPoints().at(0).state()) == 2,
            "touch point state encoding must match Qt 5.15");
    require(closeEnough(touch.touchPoints().at(0).position().x(), 9.5),
            "touch event must own a detached point payload");
    require(closeEnough(touch.touchPoints().at(0).startPosition().x(), 1.25),
            "touch start position must be preserved");
    require(closeEnough(touch.touchPoints().at(0).globalPosition().x(), 109.5),
            "touch global position must be preserved");
    require(closeEnough(touch.touchPoints().at(0).pressure(), 0.625),
            "touch pressure must be preserved");
    require(touch.touchPointStates() == Pk::TouchPointMoved
                && touch.modifiers() == Pk::ControlModifier
                && touch.timestamp() == 4321,
            "touch aggregate state, modifiers, and timestamp must be preserved");

    std::unique_ptr<PkInputEvent> clone = touch.clone();
    auto *touchClone = dynamic_cast<PkTouchEvent *>(clone.get());
    require(touchClone && touchClone != &touch, "touch clone must preserve dynamic type and ownership");
    PkList<PkTouchPoint> replacement = touchClone->touchPoints();
    replacement[0].setPosition(PkPointF(-8.0, -9.0));
    touchClone->setTouchPoints(replacement);
    require(closeEnough(touch.touchPoints().at(0).position().x(), 9.5),
            "mutating a cloned touch payload must not mutate the source");
}

class RecordingAction final : public KisAbstractInputAction
{
public:
    RecordingAction(const char *id, int actionPriority)
        : KisAbstractInputAction(PkString(id))
        , m_priority(actionPriority)
    {
    }

    void activate(int shortcut) override
    {
        ++activateCount;
        lastShortcut = shortcut;
    }

    void deactivate(int shortcut) override
    {
        ++deactivateCount;
        lastShortcut = shortcut;
    }

    void begin(int shortcut, PkInputEvent *event) override
    {
        KisAbstractInputAction::begin(shortcut, event);
        ++beginCount;
        lastShortcut = shortcut;
        beginEvent = event;
    }

    void end(PkInputEvent *event) override
    {
        ++endCount;
        endEvent = event;
    }

    void inputEvent(PkInputEvent *event) override
    {
        ++inputCount;
        KisAbstractInputAction::inputEvent(event);
    }

    int priority() const override { return m_priority; }

    void cursorMoved(const PkPointF &lastPos, const PkPointF &pos) override
    {
        lastMoveFrom = lastPos;
        lastMoveTo = pos;
    }

    PkPointF resolvedEventPosition(const PkInputEvent *event)
    {
        return eventPosF(event);
    }

    int activateCount = 0;
    int deactivateCount = 0;
    int beginCount = 0;
    int endCount = 0;
    int inputCount = 0;
    int lastShortcut = -1;
    PkInputEvent *beginEvent = nullptr;
    PkInputEvent *endEvent = nullptr;
    PkPointF lastMoveFrom;
    PkPointF lastMoveTo;

private:
    int m_priority;
};

void testShortcutMatchingAndState()
{
    RecordingAction lower("lower", 2);
    RecordingAction higher("higher", 9);
    const PkNativeGestureEvent unmappedGesture(Pk::PanNativeGesture, PkPointF(12.0, 18.0));
    const PkPointF unmappedPosition = lower.resolvedEventPosition(&unmappedGesture);
    require(closeEnough(unmappedPosition.x(), 0.0) && closeEnough(unmappedPosition.y(), 0.0),
            "native gestures without an installed mapper must preserve Qt's empty fallback");

    KisShortcutMatcher matcher;
    matcher.enterEvent();

    auto *lowerShortcut = new KisStrokeShortcut(&lower, 41);
    lowerShortcut->setButtons(PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                              PkSet<Pk::MouseButton>{Pk::LeftButton});
    auto *higherShortcut = new KisStrokeShortcut(&higher, 42);
    higherShortcut->setButtons(PkSet<Pk::Key>{Pk::Key_Shift, Pk::Key_Control},
                               PkSet<Pk::MouseButton>{Pk::LeftButton});
    matcher.addShortcut(lowerShortcut);
    matcher.addShortcut(higherShortcut);

    require(!matcher.keyPressed(Pk::Key_Shift), "modifier press must not fire an atomic action");
    require(!matcher.keyPressed(Pk::Key_Control), "second modifier press must only prepare strokes");

    PkInputEvent press(PkInputEvent::MouseButtonPress,
                       PkPointF(4.25, 7.5),
                       PkPointF(4.25, 7.5),
                       PkPointF(40.25, 70.5),
                       Pk::LeftButton,
                       Pk::LeftButton,
                       Pk::ShiftModifier | Pk::ControlModifier);
    require(matcher.buttonPressed(Pk::LeftButton, &press), "matching button must begin a stroke");
    require(lower.beginCount == 0 && higher.beginCount == 1 && higher.lastShortcut == 42,
            "the highest-priority matching stroke must win");
    require(higher.beginEvent == &press, "begin must receive the original event payload without ownership transfer");

    PkInputEvent move(PkInputEvent::MouseMove,
                      PkPointF(8.5, 11.75),
                      PkPointF(8.5, 11.75),
                      PkPointF(44.5, 74.75),
                      Pk::NoButton,
                      Pk::LeftButton,
                      Pk::ShiftModifier | Pk::ControlModifier);
    require(matcher.pointerMoved(&move), "move must be routed to the running stroke");
    require(higher.inputCount == 1
                && closeEnough(higher.lastMoveFrom.x(), 4.25)
                && closeEnough(higher.lastMoveTo.x(), 8.5),
            "default action movement tracking must preserve event coordinates");

    PkInputEvent release(PkInputEvent::MouseButtonRelease,
                         PkPointF(8.5, 11.75),
                         PkPointF(8.5, 11.75),
                         PkPointF(44.5, 74.75),
                         Pk::LeftButton,
                         Pk::NoButton,
                         Pk::ShiftModifier | Pk::ControlModifier);
    require(matcher.buttonReleased(Pk::LeftButton, &release), "matching release must end the running stroke");
    require(higher.endCount == 1 && higher.endEvent == &release && higher.deactivateCount == 1,
            "stroke release must end then deactivate the winning action");

    require(higherShortcut->priority() == 137709,
            "stroke priority must preserve modifier/button/action weighting");
}

void testSingleActionConflictAndPriority()
{
    RecordingAction action("atomic", 7);
    KisSingleActionShortcut shortcut(&action, 3);
    shortcut.setKey(PkSet<Pk::Key>{Pk::Key_Shift}, Pk::Key_A);

    require(shortcut.priority() == 10, "single-action priority must preserve modifier and action weights");
    require(shortcut.match(PkSet<Pk::Key>{Pk::Key_Shift}, Pk::Key_A),
            "single-action shortcut must match equal modifiers and key");
    require(!shortcut.match(PkSet<Pk::Key>{Pk::Key_Control}, Pk::Key_A),
            "single-action shortcut must reject a different modifier set");

    const PkKeySequence matching{static_cast<int>(Pk::SHIFT) | static_cast<int>(Pk::Key_A)};
    const PkKeySequence unrelated{static_cast<int>(Pk::CTRL) | static_cast<int>(Pk::Key_B)};
    require(!matching.isEmpty() && matching[0] == 33554497,
            "key sequence chord encoding must match Qt 5.15");
    require(shortcut.conflictsWith(matching), "equal first chord must conflict");
    require(!shortcut.conflictsWith(unrelated), "unrelated first chord must not conflict");
}

void testTouchHoldClassification()
{
    RecordingAction action("touch", 0);
    KisTouchShortcut shortcut(&action, 4, KisShortcutConfiguration::OneFingerHold);
#if defined(__APPLE__) && defined(__MACH__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
    require(!shortcut.isHoldType(), "macOS must keep native hold gestures disabled");
#else
    require(shortcut.isHoldType(), "non-macOS platforms must recognize one-finger hold gestures");
#endif
}

class TrackingNativeShortcut final : public KisNativeGestureShortcut
{
public:
    TrackingNativeShortcut(KisAbstractInputAction *action, int *destructionCount)
        : KisNativeGestureShortcut(action, 5, Pk::PanNativeGesture)
        , m_destructionCount(destructionCount)
    {
    }

    ~TrackingNativeShortcut() override { ++*m_destructionCount; }

private:
    int *m_destructionCount;
};

void testMatcherOwnsEveryRegisteredShortcut()
{
    RecordingAction action("owned", 0);
    int destructionCount = 0;
    {
        KisShortcutMatcher matcher;
        matcher.addShortcut(new TrackingNativeShortcut(&action, &destructionCount));
        matcher.enterEvent();
        PkNativeGestureEvent event(Pk::PanNativeGesture, PkPointF(2.0, 3.0));
        require(matcher.nativeGestureEvent(&event) && matcher.hasRunningShortcut(),
                "native shortcut must enter the running state before clear");
        matcher.clearShortcuts();
        require(destructionCount == 1, "clear must destroy registered native-gesture shortcuts");
        require(!matcher.hasRunningShortcut(), "clear must not leave a dangling running shortcut");
    }
    require(destructionCount == 1, "matcher must destroy registered native-gesture shortcuts");
}

} // namespace

int main()
{
    testEventPayloadsMatchQt515Oracle();
    testShortcutMatchingAndState();
    testSingleActionConflictAndPriority();
    testTouchHoldClassification();
    testMatcherOwnsEveryRegisteredShortcut();
    std::cout << "PkInputSemanticsTest: PASS\n";
    return 0;
}
