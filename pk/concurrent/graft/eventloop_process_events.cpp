// Production-shape graft for
// libs/impex/KisDocumentApplicationServices.cpp:63-68. The retained source
// repeatedly tries a lock and pumps processEvents while it is unavailable.
#include "PkEventLoop.h"
#include "PkThreadCallQueue.h"
#include "PkThread.h"

#include <mutex>

int main()
{
    std::mutex mutex;
    mutex.lock();

    const PkThreadId publishedId = PkThreadCallQueue::warmUpCurrentThread();
    PkThreadCallQueue::post(publishedId, [&] { mutex.unlock(); });

    const int processed = PkEventLoop::execUntil([&] {
        if (!mutex.try_lock()) {
            return false;
        }
        mutex.unlock();
        return true;
    });

    return processed == 1 ? 0 : 1;
}
