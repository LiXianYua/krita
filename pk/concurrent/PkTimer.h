#pragma once
#include "PkThread.h"
#include <chrono>
#include <functional>
#include <memory>

class PkTimer {
public:
    explicit PkTimer(PkThreadId target = PkThread::currentThreadId());
    ~PkTimer();
    PkTimer(const PkTimer&) = delete;
    PkTimer& operator=(const PkTimer&) = delete;

    // Starts or restarts the timer. Negative intervals are clamped to zero.
    // Every repeating timer keeps at most one callback queued: the
    // next callback is posted only after the target thread pumps and delivers
    // the current one. Positive intervals are measured again after delivery;
    // non-positive intervals run once per pump pass. Thus a stalled target
    // cannot accumulate an unbounded backlog. stop() invalidates every queued,
    // not-yet-running callback.
    void start(std::chrono::milliseconds interval, std::function<void()> callback,
               bool singleShot = false);
    void stop();
    bool isActive() const;

private:
    struct State;
    std::shared_ptr<State> m_state;
};
