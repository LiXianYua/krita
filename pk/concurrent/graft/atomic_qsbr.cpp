#include <cassert>
#include <cstdint>
#include <cstring>
#define KIS_ASSERT assert
#define Q_DISABLE_COPY(Type) Type(const Type &) = delete; Type &operator=(const Type &) = delete;
using quint64 = std::uint64_t;
using qint32 = std::int32_t;
#include "qsbr.h"

struct Target { bool called = false; void call() { called = true; } };
int main()
{
    QSBR qsbr;
    Target target;
    qsbr.lockRawPointerAccess();
    assert(qsbr.sanityRawPointerAccessLocked());
    qsbr.enqueue(&Target::call, &target);
    qsbr.unlockRawPointerAccess();
    qsbr.update();
    assert(target.called);
}
