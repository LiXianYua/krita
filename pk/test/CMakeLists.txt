cmake_minimum_required(VERSION 3.16)
project(pktest CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
add_library(pktest STATIC
    PkTestCase.cpp
    PkTestRunner.cpp
    PkTestCompare.cpp
    PkTestData.cpp
    PkTestDataString.cpp
)
target_include_directories(pktest PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# pk/string 是只给 operator<<(const char*) 非模板重载用的依赖（PkTestDataString.cpp）。
# libpktest.a 本身不依赖 pkstring：PkTestDataString.cpp 是唯一引用 PkString 的
# 目标文件，而它会被链接器按需拉入。不链 pkstring 的测试（如 pk/container 的 test_pkvector）
# 不会调用 operator<<(const char*)，因此链接器不会拉入 PkTestDataString.o，也就
# 不会引入对 pkstring 的需要。
set(PKSTRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../string)
add_subdirectory(${PKSTRING_DIR} ${CMAKE_BINARY_DIR}/pkstring EXCLUDE_FROM_ALL)

# 测试发现走 pk_test_moc.py（替代 moc），不走 Qt 的元对象系统。
# Python3 而非 C++：产物是纯 .cpp，交叉编译到 Android 时 host 侧生成器零额外工具链成本。
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(PKTEST_MOC_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/pk_test_moc.py" CACHE INTERNAL "")

# pk_test_generate(<out_var> <header>...)
# 为每个测试头生成一个 PkTestBinder<T> 特化 .cpp，把产物路径追加进 out_var。
function(pk_test_generate out_var)
    set(generated "")
    foreach(header ${ARGN})
        get_filename_component(base "${header}" NAME_WE)
        # .inc 而非 .cpp：产物全是类内定义（隐式 inline），契约上绝不能作为
        # 独立翻译单元编译，只能被调用 qExec<T> 的那个 .cpp #include 进去
        # （ODR 硬规则见下方 selftest_pktest 的说明）。扩展名把这条约束
        # 钉在文件名层面，免得日后有人写 file(GLOB "*.cpp") 原样踩回去。
        set(out "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_${base}.inc")
        add_custom_command(
            OUTPUT "${out}"
            COMMAND ${Python3_EXECUTABLE}
                    "${PKTEST_MOC_SCRIPT}" "${header}" -o "${out}"
            DEPENDS "${header}" "${PKTEST_MOC_SCRIPT}"
            COMMENT "pk_test_moc ${header}"
            VERBATIM)
        list(APPEND generated "${out}")
    endforeach()
    set(${out_var} "${generated}" PARENT_SCOPE)
endfunction()

# SelfAssertCase 的 binder 由生成器产出（对照组见 tests/generator_cases/self_assert_case.h
# 与 tests/selftest_assert.cpp 头部的说明）——证明生成器产出的 binder 与手写等价，
# 也证明 Q_OBJECT 展开成的 friend 模板真的让生成的 lambda 能访问 private 成员。
#
# CompatShapeCase 额外证明一件事：不是靠本地 #define 等价展开、而是走真实
# compat/QObject 垫片（-I pk/test/compat 下 <QObject> 解析到我们的实现）时，
# 生成器与 Q_OBJECT 展开的 friend 声明仍然严丝合缝。
pk_test_generate(PKTEST_GENERATED_SELFTEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/generator_cases/self_assert_case.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/generator_cases/compat_shape_case.h")

add_executable(selftest_pktest
    tests/selftest_main.cpp
    tests/selftest_assert.cpp
    tests/selftest_compare.cpp
    tests/selftest_expectfail.cpp
    tests/selftest_data.cpp
    tests/selftest_compat.cpp
    tests/selftest_runner.cpp
)
target_link_libraries(selftest_pktest PRIVATE pktest pkstring)

# 这条 -I 是 target 级的，selftest_pktest 的全部源文件都能看见 pk/test/compat。
# 其余源文件不受影响，靠的不是路径范围，而是它们本来就没有
# #include <QObject>/<QTest>——只有 tests/selftest_compat.cpp 写了这两个
# include，也只有它会解析到 compat/ 下的改写宏。
target_include_directories(selftest_pktest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compat)

# 生成的 binder .cpp 不作为独立翻译单元编译进可执行文件：PkTestBinder<T> 是
# 显式特化，qExec<T> 实例化处必须"看见"它的完整定义——分开编译成独立目标
# 文件的话，qExec 所在的 TU 只看得到 PkTestBinder<T> 的前置声明（不完整类型），
# 编不过。做法与 Qt moc 的 "#include moc_X.cpp" 惯例一致：调用 qExec<T> 的
# .cpp 自己 #include 生成的 binder .cpp，两者同一个 TU。
# out_var 仍返回生成产物路径——这里只用它补上构建期依赖，让生成先于编译发生。
target_include_directories(selftest_pktest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(tests/selftest_assert.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_self_assert_case.inc")
set_source_files_properties(tests/selftest_compat.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_compat_shape_case.inc")
