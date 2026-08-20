#pragma once
#include <QObject>
#include <PkTest.h>

class TestTimer : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void callbackWaitsForExplicitPump();
    void stopAndDestructionCancelCallbacks();
    void repeatingTimerPostsMoreThanOnce();
};
