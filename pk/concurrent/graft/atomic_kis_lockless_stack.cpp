#include <cstdint>
using qint32 = std::int32_t;
#define Q_DISABLE_COPY(Type) Type(const Type &) = delete; Type &operator=(const Type &) = delete;
#include "kis_lockless_stack.h"

int main()
{
    KisLocklessStack<int> stack;
    stack.push(42);
    int value = 0;
    return !stack.pop(value) || value != 42;
}
