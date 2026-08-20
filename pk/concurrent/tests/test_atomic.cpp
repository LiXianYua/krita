#include "test_atomic.h"
#include "../PkAtomic.h"
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>

namespace {
std::string atomicHeader()
{
    std::string path = __FILE__;
    path.replace(path.rfind("tests/test_atomic.cpp"), sizeof("tests/test_atomic.cpp") - 1,
                 "PkAtomic.h");
    std::ifstream input(path);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

void verifyHeaderContract(const char *fragment)
{
    PK_VERIFY(atomicHeader().find(fragment) != std::string::npos);
}
}

void TestAtomic::testDefaultConstruct()
{
    PkAtomicInt a;
    PK_COMPARE((int)a, 0);
}

void TestAtomic::testRefDeref()
{
    // 复现 kis_shared.h 的引用计数用法：ref() 返回新值非零
    PkAtomicInt ref(1);
    PK_VERIFY(ref.ref());   // 1 -> 2，非零
    PK_COMPARE((int)ref, 2);
    PK_VERIFY(ref.deref()); // 2 -> 1，非零
    PK_VERIFY(!ref.deref()); // 1 -> 0，返回 false
    PK_COMPARE((int)ref, 0);
}

void TestAtomic::testFetchAndAddOrdered()
{
    PkAtomicInt a(10);
    int old = a.fetchAndAddOrdered(5);
    PK_COMPARE(old, 10);
    PK_COMPARE((int)a, 15);
}

void TestAtomic::testFetchAndStoreOrdered()
{
    PkAtomicInt a(7);
    int old = a.fetchAndStoreOrdered(42);
    PK_COMPARE(old, 7);
    PK_COMPARE((int)a, 42);
}

void TestAtomic::testTestAndSetOrderedSuccess()
{
    PkAtomicInt a(3);
    PK_VERIFY(a.testAndSetOrdered(3, 9));
    PK_COMPARE((int)a, 9);
}

void TestAtomic::testTestAndSetOrderedFailure()
{
    PkAtomicInt a(3);
    PK_VERIFY(!a.testAndSetOrdered(999, 9));
    PK_COMPARE((int)a, 3); // 失败不改值
}

void TestAtomic::testAtomicPointerBasics()
{
    int x = 1;
    int y = 2;
    PkAtomicPointer<int> p(&x);
    PK_COMPARE((int*)p, &x);
    p = &y;
    PK_COMPARE((int*)p, &y);
}

void TestAtomic::testAtomicPointerCAS()
{
    // 复现 kis_lockless_stack.h 的 push() CAS 循环模式
    int a = 1, b = 2;
    PkAtomicPointer<int> top(&a);
    PK_VERIFY(top.testAndSetOrdered(&a, &b));
    PK_COMPARE((int*)top, &b);
    PK_VERIFY(!top.testAndSetOrdered(&a, &b)); // 当前已是 &b，用 &a 期望值会失败
}

void TestAtomic::testConcurrentRefCounting()
{
    // 复现 kis_shared.h 的多线程 ref/deref 场景：relaxed 语义下
    // 计数最终值必须正确（原子性），不依赖执行顺序。
    PkAtomicInt counter(0);
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&]{
            for (int j = 0; j < 10000; ++j) counter.ref();
        });
    }
    for (auto& t : threads) t.join();
    PK_COMPARE((int)counter, 80000);
}

void TestAtomic::testConcurrentCAS()
{
    // 复现 kis_lock_free_lod_counter.h 的 CAS 重试循环：多线程各自
    // 用 testAndSetOrdered 累加，最终值必须精确等于线程数*每线程次数。
    PkAtomicInt counter(0);
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&]{
            for (int j = 0; j < 10000; ++j) {
                int old;
                do {
                    old = (int)counter;
                } while (!counter.testAndSetOrdered(old, old + 1));
            }
        });
    }
    for (auto& t : threads) t.join();
    PK_COMPARE((int)counter, 80000);
}

void TestAtomic::testLoadStoreRelaxed()
{
    PkAtomicInt a(5);
    PK_COMPARE(a.loadRelaxed(), 5);
    a.storeRelaxed(99);
    PK_COMPARE(a.loadRelaxed(), 99);
    PK_COMPARE((int)a, 99);
}

void TestAtomic::testFetchAndAddRelaxedAcquire()
{
    // 真实调用点：libs/image/tiles3/KisTiledExtentManager.cpp 的
    // fetchAndAddRelaxed/fetchAndAddAcquire。返回值约定同 fetchAndAddOrdered
    // ——相加前的旧值。
    PkAtomicInt a(10);
    int old1 = a.fetchAndAddRelaxed(5);
    PK_COMPARE(old1, 10);
    PK_COMPARE(a.loadRelaxed(), 15);

    int old2 = a.fetchAndAddAcquire(3);
    PK_COMPARE(old2, 15);
    PK_COMPARE(a.loadRelaxed(), 18);
}

void TestAtomic::testAcquireReleaseSequencing()
{
    // release-acquire 配对的正面证明：写线程先写 payload（relaxed），
    // 再 storeRelease(ready=1)；读线程自旋 loadAcquire(ready) 直到看到
    // 1，再读 payload——release/acquire 建立的 happens-before 保证此时
    // payload 必然是写线程写入后的值，不依赖调度巧合。若 storeRelease/
    // loadAcquire 被误接成 relaxed，本测试在正确的硬件/编译器行为下仍可能
    // 偶然通过，但方法论与本文件已有的 testConcurrentCAS/
    // testConcurrentRefCounting 同——用真实跨线程交互验证值的正确性，
    // 不是空转 API 表面。
    PkAtomicInt payload(0);
    PkAtomicInt ready(0);
    std::thread writer([&]{
        payload.storeRelaxed(42);
        ready.storeRelease(1);
    });
    while (ready.loadAcquire() == 0) {
        std::this_thread::yield();
    }
    writer.join();
    PK_COMPARE(payload.loadRelaxed(), 42);
}

void TestAtomic::testAtomicPointerLoadStoreRelaxedAcquireRelease()
{
    int x = 1, y = 2;
    PkAtomicPointer<int> p(&x);
    PK_COMPARE(p.loadRelaxed(), &x);
    p.storeRelaxed(&y);
    PK_COMPARE(p.loadRelaxed(), &y);
    p.storeRelease(&x);
    PK_COMPARE(p.loadAcquire(), &x);
}

void TestAtomic::testAtomicPointerFetchAndAddRelaxedAcquire()
{
    int arr[4] = {0, 0, 0, 0};
    PkAtomicPointer<int> p(&arr[0]);
    int* old1 = p.fetchAndAddRelaxed(1);
    PK_COMPARE(old1, &arr[0]);
    PK_COMPARE(p.loadRelaxed(), &arr[1]);

    int* old2 = p.fetchAndAddAcquire(2);
    PK_COMPARE(old2, &arr[1]);
    PK_COMPARE(p.loadRelaxed(), &arr[3]);
}

void TestAtomic::testImplicitIntLoadAcquire()
{
    PkAtomicInt payload(0), ready(0);
    std::thread writer([&] { payload.storeRelaxed(42); ready = 1; });
    while ((int)ready == 0) std::this_thread::yield();
    writer.join();
    PK_COMPARE(payload.loadRelaxed(), 42);
    verifyHeaderContract("operator int() const { return m_v.load(std::memory_order_acquire); }");
}

void TestAtomic::testImplicitIntStoreRelease()
{
    verifyHeaderContract("PkAtomicInt& operator=(int value) {\n        m_v.store(value, std::memory_order_release);");
}

void TestAtomic::testRefSeqCst()
{
    PkAtomicInt value(0);
    PK_VERIFY(value.ref());
    verifyHeaderContract("m_v.fetch_add(1, std::memory_order_seq_cst)");
}

void TestAtomic::testDerefSeqCst()
{
    PkAtomicInt value(1);
    PK_VERIFY(!value.deref());
    verifyHeaderContract("m_v.fetch_sub(1, std::memory_order_seq_cst)");
}

void TestAtomic::testImplicitPointerLoadAcquire()
{
    verifyHeaderContract("operator T*() const { return m_v.load(std::memory_order_acquire); }");
}

void TestAtomic::testImplicitPointerArrowAcquire()
{
    struct Payload { int value; } payload{42};
    PkAtomicPointer<Payload> pointer;
    std::thread writer([&] { pointer = &payload; });
    while ((Payload *)pointer == nullptr) std::this_thread::yield();
    writer.join();
    PK_COMPARE(pointer->value, 42);
    verifyHeaderContract("operator->() const { return m_v.load(std::memory_order_acquire); }");
}

void TestAtomic::testImplicitPointerStoreRelease()
{
    verifyHeaderContract("PkAtomicPointer& operator=(T* value) {\n        m_v.store(value, std::memory_order_release);");
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_atomic.inc"

int run_atomic_tests(int argc, char **argv)
{
    TestAtomic tc;
    return PkTest::qExec(&tc, argc, argv);
}
