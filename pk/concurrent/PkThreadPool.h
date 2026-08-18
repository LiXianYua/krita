#pragma once
#include "PkRunnable.h"
#include <deque>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

// 替代 <QThreadPool>。方法面 = kis_updater_context.h/.cpp 实测用到的 4 个：
// start/setMaxThreadCount/maxThreadCount/waitForDone。自写通用固定线程数
// 线程池：提交数超过 maxThreadCount 时排队，不是"槽位恒等线程数"模型
// （那是调用方 KisUpdaterContext 自己的惯例，见本任务 plan"行为规格来源"
// 一节的判读结论）。
class PkThreadPool {
public:
    explicit PkThreadPool(int maxThreadCount = 1);
    ~PkThreadPool();

    PkThreadPool(const PkThreadPool&) = delete;
    PkThreadPool& operator=(const PkThreadPool&) = delete;

    void setMaxThreadCount(int count);
    int maxThreadCount() const;

    void start(PkRunnable* runnable);
    void waitForDone();

private:
    void workerLoop();
    void resizeWorkersLocked(int count);

    mutable std::mutex m_mutex;
    std::condition_variable m_taskAvailable;
    std::condition_variable m_idle;
    std::deque<PkRunnable*> m_queue;
    std::vector<std::thread> m_workers;
    int m_maxThreadCount;
    int m_busyCount = 0;
    bool m_stopping = false;
};
