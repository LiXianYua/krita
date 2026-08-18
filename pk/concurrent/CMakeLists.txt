cmake_minimum_required(VERSION 3.16)
project(pkconcurrent CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
find_package(Threads REQUIRED)

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③靠 nm -u 自证。
add_library(pkconcurrent INTERFACE)
target_include_directories(pkconcurrent INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkconcurrent INTERFACE Threads::Threads)

set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
if (NOT TARGET pktest)
    add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest)
endif()

pk_test_generate(PKCONCURRENT_BINDERS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mutex.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_rwlock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_atomic.h)

add_executable(test_pkconcurrent
    tests/test_main.cpp
    tests/test_mutex.cpp
    tests/test_rwlock.cpp
    tests/test_atomic.cpp
)
target_include_directories(test_pkconcurrent PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${PKTEST_DIR}/compat ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_pkconcurrent PRIVATE pkconcurrent pktest)
set_source_files_properties(tests/test_mutex.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_mutex.inc")
set_source_files_properties(tests/test_rwlock.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_rwlock.inc")
set_source_files_properties(tests/test_atomic.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_atomic.inc")
