#include "test_util.h"

int g_failures = 0;

void run_tree_tests();
void run_connect_tests();
void run_pointer_tests();
void run_generator_tests();

int main()
{
    run_tree_tests();
    run_connect_tests();
    run_pointer_tests();
    run_generator_tests();
    if (g_failures == 0) {
        std::printf("all pksignal tests passed\n");
        return 0;
    }
    std::printf("%d pksignal checks failed\n", g_failures);
    return 1;
}
