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
    void explicitTemplateReadEntryForms();
    void writeEntryQuint32IsUnambiguousAndRoundTrips();
    void readEntryWithStringLiteralDoesNotBindToBool();
    void twoArgConstructorFromSharedConfigHandle();
    void colorReadEntryRejectsOutOfRangeSegments();
    void deleteGroupClearsEveryKeyAndPreservesOtherGroups();
    void clearGroupMakesEveryKeyPendingWithoutMirroringBack();
    void concurrentReadsAndGroupClearsAreSafe();
    void persistsAcrossFreshProcessesAndPreservesForeignKritarcData();
    void concurrentProcessWritersDoNotLoseKeys();
    void corruptOwnedSectionFailsClosed();
    void writeFailureKeepsPendingMemoryState();
    void pureReadDoesNotManufactureTheLockFile();
    void readOnlyConfigDirectoryKeepsPersistedValues();
    void typedAndDeletionSemanticsSurviveRestart();
    void symmetricWireSizeLimitIsRetryable();
    void testPathOverrideContainsParentAndHelperWrites();
    void commitPointFailuresHaveTruthfulResults();
    void linuxFallbackMatchesResourceConfigPath();
};
