cmake_minimum_required(VERSION 3.16)
project(pkflags CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

# 本模块是纯头文件模板（PkFlags<Enum> 全部 inline），没有可编成 .a 的库目标。
# 判据③「nm -u <产物> | grep -i qt」因此没有产物可查——见 README §「判据③」，
# 改由单测二进制 test_pkflags 代查（它只链 pktest + 本模块头，链进真 Qt 会在
# ldd/nm 现形）。
add_library(pkflags INTERFACE)
target_include_directories(pkflags INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest EXCLUDE_FROM_ALL)

pk_test_generate(PKFLAGS_GENERATED_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/PkFlagsTest.h")

function(pk_test_add_executable target source test_class)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE pkflags pktest ${ARGN})
    add_test(NAME ${target} COMMAND ${target})
    target_include_directories(${target} PRIVATE
        ${PKTEST_DIR}/compat
        ${CMAKE_CURRENT_BINARY_DIR})
    set_source_files_properties(${source} PROPERTIES
        OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_${test_class}.inc")
endfunction()

pk_test_add_executable(test_pkflags tests/test_pkflags.cpp PkFlagsTest)