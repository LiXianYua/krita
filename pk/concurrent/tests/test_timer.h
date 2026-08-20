#pragma once
#include <QObject>
#include <PkTest.h>

class TestTimer : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void callbackWaitsForExplicitPump();
    void stopAndDestructionCancelCallbacks();
    void repeatingTimerPostsMoreThanOnce();
    void zeroIntervalRepeatingTimerKeepsOneCallbackOutstanding();
    void negativeIntervalRepeatingTimerClampsToZero();
    void warmedTargetThreadReceivesCallback();
    void queuedCallbackIsCancelledByStopAndDestruction();
    void queuedCallbackIsCancelledByDestructionAlone();
};
