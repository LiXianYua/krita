#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestDateTime : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultConstructedIsInvalidAndNull();
    void fromFactoryProducesValidNonNull();
    void defaultConstructedInstancesAreEqual();
    void equalityForSameAndDifferentEpoch();
    void epochSecondsRoundTrip();
    void epochMillisecondsRoundTripAtSecondBoundary();
    void millisecondsSubSecondTruncatesTowardZero();
    void secsToSignConvention();
    void currentDateTimeAndUtcAreValid();
};
