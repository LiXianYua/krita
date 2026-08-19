#include <cstdio>

// 每个测试文件导出一个 run_*_tests()，返回**失败的测试函数个数**（qExec 的
// 返回值，QTest 语义）。后续 Task 加新测试文件时各自加一行，不改这里的结构。
int run_namespace_tests();

int main()
{
    int failures = 0;
    failures += run_namespace_tests();

    if (failures == 0) {
        std::printf("all pknamespace tests passed\n");
        return 0;
    }
    std::printf("%d pknamespace test functions failed\n", failures);
    return 1;
}
