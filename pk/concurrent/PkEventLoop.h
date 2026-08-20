#pragma once

#include <functional>

// Explicit, caller-driven event-loop conveniences. Nothing in this type owns
// a thread or pumps in the background.
class PkEventLoop {
public:
    // Process exactly the queue snapshot present on entry.
    static int processEvents();

    // Pump snapshots until stopCondition becomes true. The caller is
    // responsible for arranging that posted work eventually satisfies it.
    // Returns the total number of dequeued calls attempted.
    static int execUntil(const std::function<bool()>& stopCondition);
};
