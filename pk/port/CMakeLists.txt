cmake_minimum_required(VERSION 3.16)
project(pkport CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(PK_STRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../string)
set(PK_TEST_DIR   ${CMAKE_CURRENT_SOURCE_DIR}/../test)

# 零 Qt：本工程不得出现 find_package(Qt...)。判据靠 nm -u 自证（见报告）。
#
# PkStream::errorString()/setErrorString() 要用到完整的 PkString 类型（R-01
# 交付，pk/string/）。不用 add_subdirectory(../string)：那会把 pk/string 自己
# 的 project()/test_pkstring 目标一起拉进本工程的构建图，多一层耦合、多一个
# 与本任务无关的可执行文件。直接把它的 .cpp 加进本库，效果一样，构建图更薄
# ——两种做法任务 brief 都认可，这里选的是后者。
add_library(pkport STATIC
    PkStream.cpp
    ${PK_STRING_DIR}/PkStringData.cpp
    ${PK_STRING_DIR}/PkString_core.cpp
    ${PK_STRING_DIR}/PkString_query.cpp
    ${PK_STRING_DIR}/PkString_format.cpp
)
target_include_directories(pkport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PK_STRING_DIR}
)

# 测试用 pk/test（R-11）的 PK_* 宏。同样直接加源文件、不 add_subdirectory：
# pk/test/CMakeLists.txt 会额外要求 Python3、跑代码生成、建它自己的
# selftest_pktest ——那些是 pk/test 自证自己用的，本工程只要运行时那四个
# .cpp（PkTestCase/PkTestRunner/PkTestCompare/PkTestData）。测试类的
# PkTestBinder<T> 特化手写在 tests/test_stream.cpp 里（测试方法是 public，不
# 需要 friend/Q_OBJECT），不经 pk_test_moc.py——生成器只扫 .h，而本工程的测试
# 类没有单独的头文件。
add_executable(test_pkport
    tests/test_main.cpp
    tests/test_stream.cpp
    ${PK_TEST_DIR}/PkTestCase.cpp
    ${PK_TEST_DIR}/PkTestRunner.cpp
    ${PK_TEST_DIR}/PkTestCompare.cpp
    ${PK_TEST_DIR}/PkTestData.cpp
)
target_include_directories(test_pkport PRIVATE ${PK_TEST_DIR})
target_link_libraries(test_pkport PRIVATE pkport)
