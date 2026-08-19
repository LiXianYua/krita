#include <PkTest.h>

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。所以每个测试 .cpp 自己
// #include 生成的 binder，并各自导出一个 run_xxx() 给这里调。
// 结构照抄 pk/config/tests/test_main.cpp 的骨架（两套测试都跑完、互不因对方
// 失败而被跳过，最终退出码只要任一套非 0 就报非 0）。
int run_elapsed_timer_tests(int argc, char **argv);
int run_date_time_tests(int argc, char **argv);

int main(int argc, char **argv)
{
    const int elapsedTimerResult = run_elapsed_timer_tests(argc, argv);
    const int dateTimeResult = run_date_time_tests(argc, argv);
    return (elapsedTimerResult != 0 || dateTimeResult != 0) ? 1 : 0;
}
