#pragma once

#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "PkTest.h"

class PkPainterCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void commandOrderAndPayloads();
    void nonCommutingCombinedTransform();
    void styleOverloadsConstructFreshValues();
    void penRetainsBrushAndRoundsWidth();
    void saveRestoreAndEmptyRestore();
    void copiedPathAndImagePayloadsOutliveSources();
    void measuredOverloadsSubmitOneCommand();
};
