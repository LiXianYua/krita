cmake_minimum_required(VERSION 3.16)
project(pkpointer CXX)

# 独立薄壳工程，**不接入 Krita 主构建**，也不改根 CMakeLists.txt。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据靠 tests/run_tests.sh 里的
# nm -u libpkpointer.a 自证。
add_library(pkpointer STATIC
    PkPointer.cpp
)
target_include_directories(pkpointer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# 复用 pk/test（R-11）的 harness/生成器 与 pk/container（Step 9 的 PkHash<K,V>）
# —— **只用，不改**，两者都是只读依赖。
#
# 只 add_subdirectory(pk/container) 这一处，**不**再单独 add_subdirectory(pk/test)：
# pk/container/CMakeLists.txt 内部自己已经 add_subdirectory(../test)（EXCLUDE_FROM_ALL）
# 把 pktest 静态库与 pk_test_generate() 函数（CMake 的 function 是全局作用域）
# 带出来了。同一份源目录在一次 configure 里被 add_subdirectory 两次会撞出
# "add_library cannot create target pktest because another target with the
# same name already exists"，所以这里不能像 pk/geometry 那样自己再拉一遍
# pk/test——形态上是"消费 pkcontainer 顺带拿到 pktest"，不是"消费 pktest 顺带
# 拿到 pkcontainer"。
#
# EXCLUDE_FROM_ALL：pk/container 自己的 13 个测试可执行文件与 pk/test 自己的
# selftest_pktest 都不跟着默认构建，只把 pkcontainer/pktest 两个静态库和
# pk_test_generate() 函数拉进来给 test_pkpointer 用。
# ---------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../container
                 ${CMAKE_CURRENT_BINARY_DIR}/pkcontainer
                 EXCLUDE_FROM_ALL)

pk_test_generate(PKPOINTER_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/shared_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/weak_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/scoped_case.h")

add_executable(test_pkpointer
    tests/test_main.cpp
    tests/test_shared.cpp
    tests/test_weak.cpp
    tests/test_scoped.cpp
    # 编译期 static_assert，覆盖判据 B/C 与探针 P16 的形状性质。没有 main、
    # 不贡献任何符号，能编过就是断言全部通过。
    tests/shape_asserts.cpp
)
target_link_libraries(test_pkpointer PRIVATE pkpointer pktest pkcontainer)

# 生成的 binder 是 .inc，由各 tests/test_*.cpp 自己 #include（做法与 Qt moc 的
# "#include moc_X.cpp" 惯例一致，理由见 pk/test/CMakeLists.txt 的长注释）。
# 这里只补上构建期依赖，让生成先于编译发生。
target_include_directories(test_pkpointer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(tests/test_shared.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_shared_case.inc")
set_source_files_properties(tests/test_weak.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_weak_case.inc")
set_source_files_properties(tests/test_scoped.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_scoped_case.inc")
