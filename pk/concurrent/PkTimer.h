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

    void start(std::chrono::milliseconds interval, std::function<void()> callback,
               bool singleShot = false);
    void stop();
    bool isActive() const;

private:
    struct State;
    std::shared_ptr<State> m_state;
};
