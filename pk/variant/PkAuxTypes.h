#pragma once

// Compatibility forwarding header. PkByteArray is owned by pk/container.
#include "../container/PkByteArray.h"

// Legacy direct consumers also received Qt-compatible scalar aliases through
// PkPoint.h -> PkGlobal.h. Preserve that forwarding surface without retaining
// the stale geometry dependency.
#include "../global/PkGlobal.h"
