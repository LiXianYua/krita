#include "PkTimer.h"
#include "PkThreadCallQueue.h"
#include <atomic>
#include <chrono>
#include <thread>
// Retained KisSignalCompressor ownership shape: member timer and an
// owner-capturing callback. Function-static storage exercises teardown order.
struct KisSignalCompressorShape {
    PkTimer timer;
    std::atomic<int> fires{0};
    void start(){ timer.start(std::chrono::milliseconds(2), [this]{++fires;}); }
};
KisSignalCompressorShape& staticCompressor(){ static KisSignalCompressorShape c; return c; }
int main(){
    PkThreadCallQueue::warmUpCurrentThread();
    auto &c = staticCompressor();
    c.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    PkThreadCallQueue::processPendingCalls();
    return c.fires.load() > 0 ? 0 : 1;
}
