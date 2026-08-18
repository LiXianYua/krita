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
    ${PKSTRING_DIR} ${PKCONTAINER_DIR} ${PKGEOMETRY_DIR})
target_link_libraries(pkconfig PUBLIC pkstring)

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
