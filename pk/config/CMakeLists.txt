cmake_minimum_required(VERSION 3.16)
project(pkconfig CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
#
# 复用已交付的 pk/string（PkString，真正链接的编译期依赖）、
# pk/container（PkStringList）、pk/geometry（PkPoint）——不重复实现。
#
# ⚠ pk/container 与 pk/geometry **只取头文件目录，不 add_subdirectory 它们的
# CMakeLists.txt**——这是实测出的真实限制，不是偷懒：
#   - PkStringList（PkList<PkString> 的派生类）与 PkArrayContainer/PkArrayData
#     全部是模板，方法体整份写在头文件里；libpkcontainer.a 里的 PkList.cpp
#     只显式实例化了 PkList<int>/PkList<std::string> 两个代表类型（不含
#     PkList<PkString>），本任务实际用到的 PkList<PkString> 靠隐式实例化直接
#     编进使用它的 TU——不需要链 libpkcontainer.a（pk/container/CMakeLists.txt
#     自己的注释也确认 PkStringList.h「刻意不进库」）。
#   - PkPoint/PkPointF 全部成员都是 inline/constexpr（PkPoint.cpp 里只有
#     static_assert，没有任何函数定义）——同样不需要链 libpkgeometry.a；
#     PkGlobal.h 里唯二的非 inline 符号 qIsNaN()/qInf() 本任务用不到。
#   - 反过来：pk/container/CMakeLists.txt 与 pk/geometry/CMakeLists.txt 各自都
#     无条件（没有 `NOT TARGET` 守卫）add_subdirectory 了 pk/test——两者若同时
#     被 add_subdirectory 进同一个工程，会在 "pktest"/"selftest_pktest" 这两个
#     目标名上正面相撞（CMake 目标名全工程唯一，binary dir 不同也救不了，
#     实测复现过）。这两份 CMakeLists.txt 已交付、已 VERIFIED，不能改，所以
#     只取头文件规避冲突。**Task 3 往这份文件追加内容时沿用这个模式**，不要
#     改回 add_subdirectory(pkcontainer)/add_subdirectory(pkgeometry)。
set(PKSTRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../string)
set(PKCONTAINER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../container)
set(PKGEOMETRY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../geometry)
set(PKCOLOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../color)
set(PKNAMESPACE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../namespace)

# pk/color（PkColor）：R-09 的 PkConfigColor 已退场，readEntry/writeEntry 改接
# PkColor（R-27 Task 1 交付）。PkColor 的构造/分量 getter/operator== 实现在
# PkColor.cpp（非 inline），PkColor.cpp 又引用 pk/global 的 pk_qt_assert —— 所以
# 必须真正 add_subdirectory + 链接 libpkcolor（经它 PUBLIC 链到 libpkglobal /
# libpkstring），不能像 pk/geometry 那样只取头文件目录。
#
# ⚠ add_subdirectory(pk/color) 必须放在本工程自己 add pk/string / pk/test 之前：
# pk/color 的 CMakeLists 会（经 pk/global）无条件 add 同一份 pk/test 到二进制目录
# ${CMAKE_BINARY_DIR}/pk_test、pk/string 到 ${CMAKE_BINARY_DIR}/pkstring。若本工程
# 先 add 了 pk/string / pk/test（各自二进制目录），同一源目录配不同二进制目录会
# 触发 CMake 硬错误（pk/color 自己 CMakeLists 的长注释就是这条坑的实录）。
# 先 add pk/color 让 pktest/pkstring 目标先存在，下面本工程的 `if (NOT TARGET ...)`
# 守卫自然跳过，不会重复 add。EXCLUDE_FROM_ALL：只把 libpkcolor 作为依赖拉进来，
# 不让 pk/color 的 test_pkcolor 进默认 all 目标。
if (NOT TARGET pkcolor)
    add_subdirectory(${PKCOLOR_DIR} ${CMAKE_BINARY_DIR}/pkcolor EXCLUDE_FROM_ALL)
endif()
if (NOT TARGET pkstring)
    add_subdirectory(${PKSTRING_DIR} ${CMAKE_BINARY_DIR}/pkstring)
endif()

add_library(pkconfig STATIC
    PkConfigStore.cpp
    PkConfigGroup.cpp
    PkSharedConfig.cpp
    PkMimeDatabase.cpp
)
target_include_directories(pkconfig PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PKSTRING_DIR} ${PKCONTAINER_DIR} ${PKGEOMETRY_DIR}
    ${PKCOLOR_DIR} ${PKNAMESPACE_DIR})
target_link_libraries(pkconfig PUBLIC pkstring pkcolor)

set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
if (NOT TARGET pktest)
    add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest)
endif()

pk_test_generate(PKCONFIG_BINDERS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_config_group.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_mime_database.h)

add_executable(test_pkconfig
    tests/test_main.cpp
    tests/test_config_group.cpp
    tests/test_mime_database.cpp
)
target_include_directories(test_pkconfig PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${PKTEST_DIR}/compat ${CMAKE_CURRENT_SOURCE_DIR}/compat)
target_link_libraries(test_pkconfig PRIVATE pkconfig pktest)
set_source_files_properties(tests/test_config_group.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_config_group.inc")
set_source_files_properties(tests/test_mime_database.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_mime_database.inc")
