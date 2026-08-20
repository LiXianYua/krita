#pragma once

#include <QObject>

class DataStreamCase : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultsAndStatus();
    void bareBoolMatchesQt46WireBytes();
    void rejectsNonCanonicalBoolPayloads();
    void rejectsNonCanonicalVariantNullFlags();
    void readsQt46BigEndianFixtures();
    void readsQt515BigEndianFixtures();
    void writesQt46BigEndianFixtures();
    void writesQt515BigEndianFixtures();
    void littleEndianScalarRoundTrip();
    void shortReadIsStickyAndZeroesTarget();
    void rejectsUnknownAndUserTypes();
    void readsAndWritesThroughPkStream();
    void typedNullBuiltinsRoundTrip();
    void dateTimeWireStatesRoundTrip();
    void isolatedUtf16CodeUnitsRoundTrip();
    void mutatedStringDataIsAuthoritative();
    void multiElementHashRoundTripIsSemantic();
    void userTypeFailureConsumesTypeName();
    void hostileLengthsAreRejectedBeforeAllocation();
    void containerDecodedStorageIsBounded();
    void recursiveDecodeBudgetIncludesAssociativeOverheadAndPayload();
    void variantObjectStorageIsBoundedRecursively();
    void recursiveVariantNestingIsBounded();
    void copyAssignedStringMutationSerializesDestination();
};
