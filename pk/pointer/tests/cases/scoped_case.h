#pragma once

// PkScopedPointer / PkScopedArrayPointer 的单测类。形态照 shared_case.h。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkScopedPointerCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();

    void scopedTakeTransfersOwnership();
    void scopedArrayDeletesAll();
    void scopedComparisons();
};

#undef Q_SLOTS
#undef Q_OBJECT
