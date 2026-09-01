#pragma once

#include <PkObject.h>
#include <PkThreadCallQueue.h>

#include <functional>

namespace DefaultToolDeferred {

inline void post(PkObject &owner, std::function<void()> call)
{
    PkThreadCallQueue::post(owner.thread(), std::move(call), owner.callLifetime());
}

}
