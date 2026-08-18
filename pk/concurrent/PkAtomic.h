#pragma once
#include <atomic>
#include <cstddef>

// 替代 <QAtomicInt>。方法面 = 保留范围内实测用到的 5 个：
// operator int()/operator=/ref()/deref()/fetchAndAddOrdered/
// fetchAndStoreOrdered/testAndSetOrdered。内存序判读见本任务 plan 的
// "内存序判读表"——ref()/deref() 与隐式 load/store 用 relaxed（对齐 Qt
// 默认行为），*Ordered() 系列显式方法用 seq_cst（调用点自己要求的）。
class PkAtomicInt {
public:
    PkAtomicInt(int value = 0) : m_v(value) {}

    operator int() const { return m_v.load(std::memory_order_relaxed); }
    PkAtomicInt& operator=(int value) {
        m_v.store(value, std::memory_order_relaxed);
        return *this;
    }

    // Qt 语义：返回的是自增/自减后的"新值"是否非零，不是旧值、不是
    // 成功与否。fetch_add 返回旧值，+1/-1 换算成新值再判断。
    bool ref()   { return m_v.fetch_add(1, std::memory_order_relaxed) + 1 != 0; }
    bool deref() { return m_v.fetch_sub(1, std::memory_order_relaxed) - 1 != 0; }

    // Qt 语义：返回相加/交换前的旧值。
    int fetchAndAddOrdered(int valueToAdd) {
        return m_v.fetch_add(valueToAdd, std::memory_order_seq_cst);
    }
    int fetchAndStoreOrdered(int newValue) {
        return m_v.exchange(newValue, std::memory_order_seq_cst);
    }
    // Qt 语义：expectedValue 与当前值相等才写入 newValue，返回是否成功。
    bool testAndSetOrdered(int expectedValue, int newValue) {
        return m_v.compare_exchange_strong(expectedValue, newValue,
                                            std::memory_order_seq_cst);
    }

    // R-10 final review I1 新增：显式内存序存取族。真实调用点见
    // libs/image/3rdparty/lock_free_map/qsbr.h（loadAcquire）、
    // libs/image/tiles3/kis_tile_data_store.h（loadAcquire）、
    // libs/image/tiles3/KisTiledExtentManager.cpp（storeRelease/storeRelaxed/
    // loadRelaxed/fetchAndAddRelaxed/fetchAndAddAcquire）等，均在保留范围内的
    // QAtomicInt 成员上。判读方法论同上：方法名直接点名 Qt 要求的内存序，
    // 机械映射到对应的 std::memory_order，不做加强也不做放宽。
    int loadAcquire() const { return m_v.load(std::memory_order_acquire); }
    void storeRelease(int value) { m_v.store(value, std::memory_order_release); }
    int loadRelaxed() const { return m_v.load(std::memory_order_relaxed); }
    void storeRelaxed(int value) { m_v.store(value, std::memory_order_relaxed); }
    // Qt 语义：返回相加前的旧值（同 fetchAndAddOrdered 的返回值约定）。
    int fetchAndAddRelaxed(int valueToAdd) {
        return m_v.fetch_add(valueToAdd, std::memory_order_relaxed);
    }
    int fetchAndAddAcquire(int valueToAdd) {
        return m_v.fetch_add(valueToAdd, std::memory_order_acquire);
    }

private:
    std::atomic<int> m_v;
};

// 替代 <QAtomicPointer<T>>。方法面同上，指针版本。
template <class T>
class PkAtomicPointer {
public:
    PkAtomicPointer(T* value = nullptr) : m_v(value) {}

    operator T*() const { return m_v.load(std::memory_order_relaxed); }
    T* operator->() const { return m_v.load(std::memory_order_relaxed); }
    PkAtomicPointer& operator=(T* value) {
        m_v.store(value, std::memory_order_relaxed);
        return *this;
    }

    T* fetchAndStoreOrdered(T* newValue) {
        return m_v.exchange(newValue, std::memory_order_seq_cst);
    }
    bool testAndSetOrdered(T* expectedValue, T* newValue) {
        return m_v.compare_exchange_strong(expectedValue, newValue,
                                            std::memory_order_seq_cst);
    }

    // R-10 final review I1 新增：显式内存序存取族，同 PkAtomicInt 的判读
    // 方法论。真实调用点例如 libs/global/KisLazySharedCacheStorage.h:170
    // 的 storeRelaxed()（在 QAtomicPointer 成员上）。
    T* loadAcquire() const { return m_v.load(std::memory_order_acquire); }
    void storeRelease(T* value) { m_v.store(value, std::memory_order_release); }
    T* loadRelaxed() const { return m_v.load(std::memory_order_relaxed); }
    void storeRelaxed(T* value) { m_v.store(value, std::memory_order_relaxed); }
    // Qt 语义：返回相加前的旧指针值（同 fetchAndStoreOrdered 的返回值约定）。
    T* fetchAndAddRelaxed(std::ptrdiff_t valueToAdd) {
        return m_v.fetch_add(valueToAdd, std::memory_order_relaxed);
    }
    T* fetchAndAddAcquire(std::ptrdiff_t valueToAdd) {
        return m_v.fetch_add(valueToAdd, std::memory_order_acquire);
    }

private:
    std::atomic<T*> m_v;
};
