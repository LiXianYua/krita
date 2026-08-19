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

    // R-16 Task 3：字符串转换
    void fromStringYyyyBoundary();
    void fromStringYyyyMMBoundary();
    void fromStringYyyyMMddBoundary();
    void fromStringYyyyMMddThhMmBoundary();
    void fromStringYyyyMMddThhMmSsBoundary();
    void fromStringCustomFormatRejectsIllegalInput();
    void fromStringCustomFormatEmptyStringIsNull();
    void fromStringDefaultParsesTextDateShape();
    void fromStringDefaultRejectsGarbage();
    void fromStringIsoDateMarkerMatchesCustomFormat();
    void toStringDefaultMatchesTextDateShape();
    void toStringIsoDate();
    void toStringIsoDateWithMs();
    void toStringRfc2822DateFixedUtcOffset();
    void toStringOnInvalidReturnsEmpty();
};
