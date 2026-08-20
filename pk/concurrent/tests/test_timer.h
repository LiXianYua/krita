#pragma once
#include <QObject>
#include <PkTest.h>

class TestTimer : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void callbackWaitsForExplicitPump();
    void stopAndDestructionCancelCallbacks();
    void positiveRepeatingTimerKeepsOneCallbackOutstanding();
    void zeroIntervalRepeatingTimerKeepsOneCallbackOutstanding();
    void negativeIntervalRepeatingTimerClampsToZero();
    void zeroIntervalSingleShotRestartPreservesReplacementGeneration();
    void warmedTargetThreadReceivesCallback();
    void queuedCallbackIsCancelledByStopAndDestruction();
    void queuedCallbackIsCancelledByDestructionAlone();
};
