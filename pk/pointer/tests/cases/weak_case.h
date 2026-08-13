#pragma once

// PkWeakPointer 的单测类。形态照 shared_case.h。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkWeakPointerCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();

    void weakLifecycle();
    void weakOfNullValuedSharedIsNull();
    void promoteFromWeak();
};

#undef Q_SLOTS
#undef Q_OBJECT
