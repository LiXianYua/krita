cmake_minimum_required(VERSION 3.16)
project(pknamespace CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

# 本模块是纯头文件（PkNamespace.h 全部 inline，PkGlobal.h 标量也是 header-only 的
# constexpr inline），没有可编成 .a 的库目标。判据③「nm -u <产物> | grep -i qt」
# 因此没有库产物可查——见 README「判据③」，改由单测二进制 test_pknamespace 代查
# （它只链 pktest + 本模块头，链进真 Qt 会在 nm 现形）。
add_library(pknamespace INTERFACE)
target_include_directories(pknamespace INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest EXCLUDE_FROM_ALL)

pk_test_generate(PKNAMESPACE_GENERATED_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/namespace_case.h")

add_executable(test_pknamespace
    tests/test_main.cpp
    tests/test_namespace.cpp
)
target_link_libraries(test_pknamespace PRIVATE pknamespace pktest)
add_test(NAME test_pknamespace COMMAND test_pknamespace)

# 生成的 binder 是 .inc，由 test_namespace.cpp 自己 #include（做法与 Qt moc 的
# "#include moc_X.cpp" 惯例一致）。这里只补上构建期依赖，让生成先于编译发生。
target_include_directories(test_pknamespace PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(tests/test_namespace.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_namespace_case.inc")
