#include "PkTimer.h"
#include "PkThreadCallQueue.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

struct PkTimer::State : std::enable_shared_from_this<PkTimer::State> {
    explicit State(PkThreadId id) : target(id) {}
    PkThreadId target;
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    std::atomic<bool> active{false};
    std::atomic<unsigned long long> generation{0};

    void postZeroInterval(std::function<void()> callback,
                          bool singleShot,
                          unsigned long long expectedGeneration)
    {
        PkThreadCallQueue::post(target,
                                [weak = weak_from_this(), callback = std::move(callback),
                                 singleShot, expectedGeneration] {
            auto current = weak.lock();
            if (!current || !current->active.load() ||
                current->generation.load() != expectedGeneration) {
                return;
            }

            callback();
            if (singleShot) {
                current->active = false;
            } else if (current->active.load() &&
                       current->generation.load() == expectedGeneration) {
                current->postZeroInterval(callback, false, expectedGeneration);
            }
        });
    }
};

PkTimer::PkTimer(PkThreadId target) : m_state(std::make_shared<State>(target)) {}
PkTimer::~PkTimer() { stop(); }

void PkTimer::start(std::chrono::milliseconds interval, std::function<void()> callback,
                    bool singleShot)
{
    stop();
    auto state = m_state;
    const auto generation = ++state->generation;
    state->active = true;
    if (interval <= std::chrono::milliseconds::zero()) {
        state->postZeroInterval(std::move(callback), singleShot, generation);
        return;
    }
    state->worker = std::thread([state, interval, callback = std::move(callback),
                                 singleShot, generation] {
        std::unique_lock<std::mutex> lock(state->mutex);
        do {
            if (state->wake.wait_for(lock, interval, [&] {
                    return !state->active.load() || state->generation.load() != generation;
                })) return;
            PkThreadCallQueue::post(state->target, [weak = std::weak_ptr<State>(state),
                                                    callback, generation] {
                auto current = weak.lock();
                if (current && current->generation.load() == generation) callback();
            });
            if (singleShot) {
                state->active = false;
                return;
            }
        } while (state->active.load() && state->generation.load() == generation);
    });
}

void PkTimer::stop()
{
    auto state = m_state;
    state->active = false;
    ++state->generation;
    state->wake.notify_all();
    if (state->worker.joinable()) state->worker.join();
}

bool PkTimer::isActive() const { return m_state->active.load(); }
