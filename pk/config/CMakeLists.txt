cmake_minimum_required(VERSION 3.16)
project(pkconfig CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
#
# 复用已交付的 pk/string（PkString，真正链接的编译期依赖）、
# pk/container（PkStringList）、pk/geometry（PkPoint）——不重复实现。
#
# pk/geometry 只取头文件目录；PkPoint/PkPointF 全部成员都是
# inline/constexpr（PkPoint.cpp 里只有
#     static_assert，没有任何函数定义）——同样不需要链 libpkgeometry.a；
#     PkGlobal.h 里唯二的非 inline 符号 qIsNaN()/qInf() 本任务用不到。
# pk/container 必须实际链接：PkString 的实现会引用 PkByteArray 的非 inline
# 构造函数。使用其 library-only 模式可取得 canonical target 而不引入重复测试。
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
if (NOT TARGET pkcontainer)
    set(PKCONTAINER_LIBRARY_ONLY ON)
    add_subdirectory(${PKCONTAINER_DIR} ${CMAKE_BINARY_DIR}/pkcontainer EXCLUDE_FROM_ALL)
endif()
if (NOT TARGET pkstring)
    add_subdirectory(${PKSTRING_DIR} ${CMAKE_BINARY_DIR}/pkstring)
endif()

set(PKCONFIG_SOURCES
    PkConfigStore.cpp
    PkConfigGroup.cpp
    PkSharedConfig.cpp
    PkMimeDatabase.cpp
)
add_library(pkconfig_objects OBJECT ${PKCONFIG_SOURCES})
set_target_properties(pkconfig_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    AUTOMOC OFF)
target_include_directories(pkconfig_objects PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PKSTRING_DIR} ${PKCONTAINER_DIR} ${PKGEOMETRY_DIR}
    ${PKCOLOR_DIR} ${PKNAMESPACE_DIR})

# Production users keep one process-wide store in the shared library. The
# frozen S-line artifact is made from the exact same object files and is never
# linked back into production, so it restores libpkconfig.a without compiling
# or running a second logical implementation.
add_library(pkconfig SHARED $<TARGET_OBJECTS:pkconfig_objects>)
set_target_properties(pkconfig PROPERTIES AUTOMOC OFF)
target_include_directories(pkconfig PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PKSTRING_DIR} ${PKCONTAINER_DIR} ${PKGEOMETRY_DIR}
    ${PKCOLOR_DIR} ${PKNAMESPACE_DIR})
target_link_libraries(pkconfig PUBLIC pkstring pkcontainer pkcolor)

add_library(pkconfig_frozen_archive STATIC $<TARGET_OBJECTS:pkconfig_objects>)
set_target_properties(pkconfig_frozen_archive PROPERTIES AUTOMOC OFF)
if (WIN32)
    # Avoid colliding with pkconfig.dll's import library.
    set_target_properties(pkconfig_frozen_archive PROPERTIES OUTPUT_NAME pkconfig_frozen)
else()
    set_target_properties(pkconfig_frozen_archive PROPERTIES OUTPUT_NAME pkconfig)
endif()
target_include_directories(pkconfig_frozen_archive PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PKSTRING_DIR} ${PKCONTAINER_DIR} ${PKGEOMETRY_DIR}
    ${PKCOLOR_DIR} ${PKNAMESPACE_DIR})
target_link_libraries(pkconfig_frozen_archive PUBLIC pkstring pkcontainer pkcolor)

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
add_executable(pkconfig_process_helper
    tests/pkconfig_process_helper.cpp
)
target_link_libraries(pkconfig_process_helper PRIVATE pkconfig)
target_include_directories(test_pkconfig PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${PKTEST_DIR}/compat ${CMAKE_CURRENT_SOURCE_DIR}/compat)
target_link_libraries(test_pkconfig PRIVATE pkconfig pktest)
target_compile_definitions(test_pkconfig PRIVATE
    PKCONFIG_PROCESS_HELPER_PATH="$<TARGET_FILE:pkconfig_process_helper>")
add_dependencies(test_pkconfig pkconfig_process_helper)
set_source_files_properties(tests/test_config_group.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_config_group.inc")
set_source_files_properties(tests/test_mime_database.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_mime_database.inc")
