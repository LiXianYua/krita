#include "selftest_util.h"

int g_selftestFailures = 0;

void run_assert_selftests();
void run_compare_selftests();
void run_expectfail_selftests();
void run_data_selftests();

int main()
{
    run_assert_selftests();
    run_compare_selftests();
    run_expectfail_selftests();
    run_data_selftests();
    if (g_selftestFailures == 0) {
        std::printf("all pktest selftests passed\n");
        return 0;
    }
    std::printf("%d pktest selftest checks failed\n", g_selftestFailures);
    return 1;
}
