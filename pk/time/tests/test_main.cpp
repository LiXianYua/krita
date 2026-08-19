#include <PkTest.h>

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。所以测试 .cpp 自己
// #include 生成的 binder，并导出一个 run_xxx() 给这里调。
// 后续 Task（PkDateTime）会往这里追加 run_date_time_tests() 一类的调用，
// 结构照抄 pk/config/tests/test_main.cpp 的骨架。
int run_elapsed_timer_tests(int argc, char **argv);

int main(int argc, char **argv)
{
    return run_elapsed_timer_tests(argc, argv);
}
