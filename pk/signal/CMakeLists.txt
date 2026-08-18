cmake_minimum_required(VERSION 3.16)
project(pksignal CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
add_library(pksignal STATIC
    PkObject.cpp
)
target_include_directories(pksignal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(PKCONCURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../concurrent)
if (NOT TARGET pkconcurrent)
    add_subdirectory(${PKCONCURRENT_DIR} ${CMAKE_BINARY_DIR}/pkconcurrent)
endif()
target_link_libraries(pksignal PUBLIC pkconcurrent)

# Task 4：信号生成器产物。test_generator.cpp 需要生成器从自测输入生成的
# gen_signals.inc（信号成员函数定义，必须进该 TU）。custom_command 用 Python3
# 调 pk_signal_moc.py，让 CMake 从头构建也能拿到产物——这是「形态自证」，
# 不能只靠 g++ 直编时手动预先生成的那份。
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(GEN_SIGNALS_INPUT ${CMAKE_CURRENT_SOURCE_DIR}/tests/generator_cases/basic_signals.h)
set(GEN_SIGNALS_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen_signals.inc)
add_custom_command(
    OUTPUT ${GEN_SIGNALS_OUTPUT}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/pk_signal_moc.py
            ${GEN_SIGNALS_INPUT} -o ${GEN_SIGNALS_OUTPUT}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/pk_signal_moc.py ${GEN_SIGNALS_INPUT}
    COMMENT "pk_signal_moc: generating signal definitions"
)
add_custom_target(gen_signals DEPENDS ${GEN_SIGNALS_OUTPUT})

add_executable(test_pksignal
    tests/test_main.cpp
    tests/test_tree.cpp
    tests/test_connect.cpp
    tests/test_pointer.cpp
    tests/test_generator.cpp
    tests/test_thread.cpp
)
target_link_libraries(test_pksignal PRIVATE pksignal)
target_compile_definitions(test_pksignal PRIVATE
    PK_SIGNAL_GEN_INC="${GEN_SIGNALS_OUTPUT}")
add_dependencies(test_pksignal gen_signals)
