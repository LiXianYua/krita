#include <cstdio>

// 每个测试文件导出一个 run_*_tests()，返回**失败的测试函数个数**（qExec 的
// 返回值，QTest 语义）。后续 Task 加 PkPoint/PkSize/PkRect/PkTransform 时
// 各自加一行，不改这里的结构。
int run_global_tests();
int run_point_tests();
int run_size_tests();
int run_rect_tests();
int run_rectf_tests();
int run_transform_tests();
int run_line_tests();
int run_margins_tests();
int run_polygon_tests();
int run_vectornd_tests();

int main()
{
    int failures = 0;
    failures += run_global_tests();
    failures += run_point_tests();
    failures += run_size_tests();
    failures += run_rect_tests();
    failures += run_rectf_tests();
    failures += run_transform_tests();
    failures += run_line_tests();
    failures += run_margins_tests();
    failures += run_polygon_tests();
    failures += run_vectornd_tests();

    if (failures == 0) {
        std::printf("all pkgeometry tests passed\n");
        return 0;
    }
    std::printf("%d pkgeometry test functions failed\n", failures);
    return 1;
}
