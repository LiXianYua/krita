#include <PkTest.h>

// 每个测试 .cpp 自己定义 run_xxx() 给这里调。
int run_mutex_tests(int argc, char **argv);
int run_rwlock_tests(int argc, char **argv);
int run_atomic_tests(int argc, char **argv);
int run_threadpool_tests(int argc, char **argv);
int run_semaphore_tests(int argc, char **argv);
int run_threadqueue_tests(int argc, char **argv);
int run_timer_tests(int argc, char **argv);

int main(int argc, char **argv)
{
    // qExec 的返回值是失败个数（0 = 全过）。六套测试都要跑完、互不因对方
    // 失败而被跳过，最终退出码只要任一套非 0 就报非 0。
    const int mutexResult = run_mutex_tests(argc, argv);
    const int rwlockResult = run_rwlock_tests(argc, argv);
    const int atomicResult = run_atomic_tests(argc, argv);
    const int threadpoolResult = run_threadpool_tests(argc, argv);
    const int semaphoreResult = run_semaphore_tests(argc, argv);
    const int threadqueueResult = run_threadqueue_tests(argc, argv);
    const int timerResult = run_timer_tests(argc, argv);
    return (mutexResult != 0 || rwlockResult != 0 || atomicResult != 0 ||
            threadpoolResult != 0 || semaphoreResult != 0 ||
            threadqueueResult != 0 || timerResult != 0) ? 1 : 0;
}
