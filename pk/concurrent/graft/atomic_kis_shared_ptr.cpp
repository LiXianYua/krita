#include <cassert>
#define Q_ASSERT assert
#include "kis_shared.h"

KisShared::KisShared() : _ref(0), _sharedWeakReference(nullptr) {}
KisShared::~KisShared() { delete _sharedWeakReference; }

struct Item : KisShared {};
int main()
{
    Item item;
    assert(item.ref());
    assert(!item.deref());
}
