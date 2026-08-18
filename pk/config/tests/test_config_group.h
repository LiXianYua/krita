#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestConfigGroup : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void storeBasicGetSet();
    void readWriteAllTypes();
    void hasKeyDeleteEntrySync();
    void sameGroupNameSharesStorage();
    void emptyStringListDiffersFromMissingKey();
    void doubleRoundTripsBeyondSixDecimals();
};
