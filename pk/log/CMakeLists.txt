cmake_minimum_required(VERSION 3.16)
project(pklog CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
#
# 第三方依赖的获取方式由线级 spec 的「已裁决的岔路」定：装了就用系统的，
# 没装才下载，tag + hash 钉死。**不 vendor 源码进 fork**。
# I-02 把 spdlog 装进依赖前缀之后，find_package 那条自动生效，本文件不用改。
find_package(spdlog QUIET)
if (NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        79524ddd08a4ec981b7fea76afd08ee05f83755d  # v1.17.0
        GIT_SHALLOW    FALSE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL       OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(spdlog)
endif()

add_library(pklog STATIC
    PkLogSink.cpp
    PkLogBackend.cpp
    PkDebug.cpp
    PkLoggingCategory.cpp
    PkMessageLogger.cpp
)
target_include_directories(pklog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# PRIVATE：spdlog 只被 PkLogBackend.cpp 看见，pklog 的使用者对它一无所知。
target_link_libraries(pklog PRIVATE spdlog::spdlog)

# 测试用 R-11 的 PK_* harness。只是消费它，一个字节都不改。
# add_subdirectory 给了显式 binary dir，所以兄弟目录也能挂进来。
# 副作用：pk/test 的 selftest_pktest 会跟着一起编、pk_test_generate() 函数变得可用。
set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest)

# 每个测试类的 binder 由 pk/test 的生成器产出（替代 moc）。
pk_test_generate(PKLOG_BINDERS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sink.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stream.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_category.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compat.h)

add_executable(test_pklog
    tests/test_main.cpp
    tests/test_sink.cpp
    tests/test_stream.cpp
    tests/test_category.cpp
    tests/test_compat.cpp
)
target_link_libraries(test_pklog PRIVATE pklog pktest)
# ${PKTEST_DIR}/compat：让测试头里的 `#include <QObject>` 解析到 pk/test 的垫片，
# Q_OBJECT / private Q_SLOTS: 才有定义（生成器靠这两个字面 token 认测试类）。
# compat：Task 4 本任务的垫片——只有 tests/test_compat.cpp 写了
# `#include <QDebug>`/`#include <QLoggingCategory>`，也只有它会解析到这里；
# 其余源文件不受影响（它们本来就没写这两个 include）。target 级而不是全局，
# 照 pk/test/CMakeLists.txt:68-72 的做法：别让全部源文件都看见垫片，那会把
# "能不能编过" 的压力错误地转移掉。
target_include_directories(test_pklog PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${PKTEST_DIR}/compat ${CMAKE_CURRENT_SOURCE_DIR}/compat)
set_source_files_properties(tests/test_sink.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_sink.inc")
set_source_files_properties(tests/test_stream.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_stream.inc")
set_source_files_properties(tests/test_category.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_category.inc")
set_source_files_properties(tests/test_compat.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_compat.inc")
