#include "PkEventLoop.h"

#include "PkThreadCallQueue.h"

#include <thread>

int PkEventLoop::processEvents()
{
    return PkThreadCallQueue::processPendingCalls();
}

int PkEventLoop::execUntil(const std::function<bool()>& stopCondition)
{
    int processed = 0;
    while (!stopCondition()) {
        const int current = processEvents();
        processed += current;
        if (current == 0) {
            std::this_thread::yield();
        }
    }
    return processed;
}
