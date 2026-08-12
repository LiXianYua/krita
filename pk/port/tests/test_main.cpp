// 入口。测试类与手写 PkTestBinder<T> 特化分别在 test_stream.cpp /
// test_eventsink.cpp 里（照 pk/string/tests/test_main.cpp 的形态：main 只
// 转发到测试实现所在的 .cpp 里的一个函数，不重复声明测试类本身）。R-12
// Task 3 加入 test_eventsink.cpp 后，main 依次跑两套测试，任何一套失败就
// 短路返回非零——不需要为此改 run_tests.sh。
int run_stream_tests(int argc, char **argv);
int run_eventsink_tests(int argc, char **argv);

int main(int argc, char **argv)
{
    int rc = run_stream_tests(argc, argv);
    if (rc != 0) {
        return rc;
    }
    return run_eventsink_tests(argc, argv);
}
