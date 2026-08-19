#include <PkTest.h>

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。所以每个测试 .cpp 自己
// #include 生成的 binder，并各自导出一个 run_xxx() 给这里调。
int run_error_tests(int argc, char **argv);
int run_database_tests(int argc, char **argv);

int main(int argc, char **argv)
{
    int rc = 0;
    rc |= run_error_tests(argc, argv);
    rc |= run_database_tests(argc, argv);
    return rc;
}
