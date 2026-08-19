cmake_minimum_required(VERSION 3.16)
project(pkcolor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u -C test_pkcolor 自证
# （见 README「判据③」）。

# 目录变量全部归一化成绝对路径：pk/test 内部也会 add_subdirectory 同一份 pk/string，
# 相对形态不一致会让 CMake 误判为「同一二进制目录对应两个源目录」而报错。
get_filename_component(PKSTRING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../string" ABSOLUTE)
get_filename_component(PKCONTAINER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../container" ABSOLUTE)
get_filename_component(PKGLOBAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../global" ABSOLUTE)
get_filename_component(PKTEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../test" ABSOLUTE)

# 复用已交付的 pk/string（PkString，name()/setNamedColor 的返回/参数形态）、
# pk/global（标量）、pk/namespace（Qt::GlobalColor）、pk/container（PkString 的
# 存储）。PkColor.h/PkColor.cpp 走相对 include 链（../global ../namespace
# ../string → ../container）自足，这里拉目标只为拿 pkstring 的 PUBLIC 头目录与
# 链接。
#
# ⚠ 不自已 add_subdirectory(pk/string)：pk/test 的 CMakeLists 内部会无条件
# add_subdirectory 同一份 pk/string 到同一二进制目录 pkstring——同一源目录对同一
# 二进制目录只能 add 一次（CMake 实测：第二次调用直接报错，连 EXCLUDE_FROM_ALL
# 选项一致也不行）。同样地 pk/global 的 CMakeLists 内部会无条件 add_subdirectory
# 同一份 pk/test（到二进制目录 pk_test），若我这边先 add pktest 再 add pkglobal，
# 就会撞上「add_library(pktest) 已存在」的重复目标错误。
#
# 因此必须**先** add pkglobal：它连带把 pktest（经 ../test）与 pkstring（经 pktest
# 内部）两个目标一起建出来，后面的兜底守卫都不会触发。
if (NOT TARGET pkglobal)
    add_subdirectory(${PKGLOBAL_DIR} ${CMAKE_BINARY_DIR}/pkglobal EXCLUDE_FROM_ALL)
endif()
if (NOT TARGET pktest)
    # 兜底：正常流程到不了（pkglobal 无条件 add pk/test）。万一将来 pkglobal 拆掉
    # 这个依赖，这里仍能自足。
    add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest EXCLUDE_FROM_ALL)
endif()
if (NOT TARGET pkstring)
    # 兜底：正常流程到不了（pktest 无条件 add pk/string）。
    add_subdirectory(${PKSTRING_DIR} ${CMAKE_BINARY_DIR}/pkstring EXCLUDE_FROM_ALL)
endif()

add_library(pkcolor STATIC PkColor.cpp)
target_include_directories(pkcolor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PKSTRING_DIR}
    ${PKCONTAINER_DIR}
    ${PKGLOBAL_DIR}
)
target_link_libraries(pkcolor PUBLIC pkstring pkglobal)

# pk_test_generate 是 pktest 的 CMakeLists 定义的函数——上面的 add_subdirectory
# 已把 pktest 加进来，这里直接可用。
pk_test_generate(PKCOLOR_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/cases/color_case.h")

add_executable(test_pkcolor
    tests/test_main.cpp
    tests/test_color.cpp
)
target_include_directories(test_pkcolor PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${PKTEST_DIR}/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
)
target_link_libraries(test_pkcolor PRIVATE pkcolor pktest)
add_test(NAME test_pkcolor COMMAND test_pkcolor)

# 生成的 binder 是 .inc，由 test_color.cpp 自己 #include（做法与 Qt moc 的
# "#include moc_X.cpp" 惯例一致）。这里只补上构建期依赖，让生成先于编译发生。
set_source_files_properties(tests/test_color.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_color_case.inc")
