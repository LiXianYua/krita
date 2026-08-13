#include <cstdio>

// 每个测试文件导出一个 run_*_tests()，返回**失败的测试函数个数**（qExec 的
// 返回值，QTest 语义）。形态照 pk/geometry/tests/test_main.cpp。
int run_shared_tests();
int run_weak_tests();
int run_scoped_tests();

int main()
{
    int failures = 0;
    failures += run_shared_tests();
    failures += run_weak_tests();
    failures += run_scoped_tests();

    if (failures == 0) {
        std::printf("all pkpointer tests passed\n");
        return 0;
    }
    std::printf("%d pkpointer test functions failed\n", failures);
    return 1;
}
