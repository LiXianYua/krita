#include "selftest_util.h"

int g_selftestFailures = 0;

void run_assert_selftests();

int main()
{
    run_assert_selftests();
    if (g_selftestFailures == 0) {
        std::printf("all pktest selftests passed\n");
        return 0;
    }
    std::printf("%d pktest selftest checks failed\n", g_selftestFailures);
    return 1;
}
