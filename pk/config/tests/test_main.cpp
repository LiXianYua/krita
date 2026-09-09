#include <PkTest.h>
#include "../PkConfigStore.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。所以每个测试 .cpp 自己
// #include 生成的 binder，并各自导出一个 run_xxx() 给这里调。
int run_config_group_tests(int argc, char **argv);
int run_mime_database_tests(int argc, char **argv);

int main(int argc, char **argv)
{
    namespace fs = std::filesystem;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const fs::path configRoot = fs::temp_directory_path() /
        ("pkconfig-suite-" + std::to_string(stamp));
    std::error_code error;
    fs::create_directories(configRoot, error);
    if (error) {
        return 2;
    }
    if (!PkConfigStore::setConfigFilePathForTesting(configRoot / "kritarc")) {
        return 2;
    }

    // qExec 的返回值是失败个数（0 = 全过）。两套测试都要跑完、互不因对方
    // 失败而被跳过，最终退出码只要任一套非 0 就报非 0。
    const int configGroupResult = run_config_group_tests(argc, argv);
    const int mimeDatabaseResult = run_mime_database_tests(argc, argv);
    const bool synced = PkConfigStore::instance().sync();
    fs::remove_all(configRoot, error);
    return (configGroupResult != 0 || mimeDatabaseResult != 0 || !synced) ? 1 : 0;
}
