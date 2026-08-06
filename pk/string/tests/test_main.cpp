#include "test_util.h"

int g_failures = 0;

void run_core_tests();
void run_query_tests();
void run_format_tests();

int main()
{
    run_core_tests();
    run_query_tests();
    run_format_tests();
    if (g_failures == 0) {
        std::printf("all pkstring tests passed\n");
        return 0;
    }
    std::printf("%d pkstring checks failed\n", g_failures);
    return 1;
}
