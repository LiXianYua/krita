#include <PkTest.h>

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。所以每个测试 .cpp 自己
// #include 生成的 binder，并各自导出一个 run_xxx() 给这里调。
int run_config_group_tests(int argc, char **argv);
int run_mime_database_tests(int argc, char **argv);

int main(int argc, char **argv)
{
    // qExec 的返回值是失败个数（0 = 全过）。两套测试都要跑完、互不因对方
    // 失败而被跳过，最终退出码只要任一套非 0 就报非 0。
    const int configGroupResult = run_config_group_tests(argc, argv);
    const int mimeDatabaseResult = run_mime_database_tests(argc, argv);
    return (configGroupResult != 0 || mimeDatabaseResult != 0) ? 1 : 0;
}
