#include "test_atomic.h"
#include "../PkAtomic.h"
#include <thread>
#include <vector>

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

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_atomic.inc"

int run_atomic_tests(int argc, char **argv)
{
    TestAtomic tc;
    return PkTest::qExec(&tc, argc, argv);
}
