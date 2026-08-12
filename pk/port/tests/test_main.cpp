// 入口。测试类与手写 PkTestBinder<PkStreamTestCase> 特化都在 test_stream.cpp
// 里（照 pk/string/tests/test_main.cpp 的形态：main 只转发到测试实现所在的
// 那个 .cpp 里的一个函数，不重复声明测试类本身）。
int run_stream_tests(int argc, char **argv);

int main(int argc, char **argv)
{
    return run_stream_tests(argc, argv);
}
