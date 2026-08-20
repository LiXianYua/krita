#include <cassert>
#include <QtGlobal>

#define Q_INLINE_TEMPLATE inline
struct GraftDebugStub {
    GraftDebugStub &noquote() { return *this; }
    template<class T> GraftDebugStub &operator<<(const T &) { return *this; }
};
static GraftDebugStub warnKrita;
static int kisBacktrace() { return 0; }

#include "kis_shared.h"
#include "kis_shared_ptr.h"

KisShared::KisShared() : _ref(0), _sharedWeakReference(nullptr) {}
KisShared::~KisShared() { delete _sharedWeakReference; }

struct Item : KisShared {
    explicit Item(int *destructions) : destructions(destructions) {}
    ~Item() { ++*destructions; }

    int *destructions;
};

int main()
{
    int destructions = 0;
    {
        KisSharedPtr<Item> first(new Item(&destructions));
        assert(first->refCount() == 1);
        {
            KisSharedPtr<Item> second(first);
            assert(first->refCount() == 2);
            assert(second.data() == first.data());
        }
        assert(first->refCount() == 1);
    }
    assert(destructions == 1);
}
