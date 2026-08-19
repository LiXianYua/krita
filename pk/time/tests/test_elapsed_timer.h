#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestElapsedTimer : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultConstructedIsInvalid();
    void startMakesValidAndElapsedNearZero();
    void elapsedIsMonotonicAfterRealSleep();
    void nsecsElapsedGrowsAfterRealSleep();
    void nsecsToMillisecondsConversionRelation();
    void restartResetsElapsedToZero();
    void invalidateMakesInvalid();
};
