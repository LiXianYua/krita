cmake_minimum_required(VERSION 3.16)
project(pkxml CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
#
# 第三方依赖的获取方式由线级 spec 的「已裁决的岔路」定：装了就用系统的，
# 没装才下载，tag + hash 钉死。**不 vendor 源码进 fork**。
#
# pugixml 版本选取：R-07 Task 1 开工时用 `git ls-remote --tags` 现查的真实最新
# 稳定 tag 是 v1.16（commit c8033ce9d039e7f9d134877c363397b3cfe20816），见本任务
# commit message 里贴的原始 `git ls-remote` 输出。这台机器当时没有系统装的
# pugixml（`pkg-config --exists pugixml` 为假），find_package 分支留给以后
# I-02 之类的步骤把 pugixml 装进依赖前缀时自动生效，本文件不用改。
find_package(pugixml QUIET)
if (NOT pugixml_FOUND)
    include(FetchContent)
    FetchContent_Declare(pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG        c8033ce9d039e7f9d134877c363397b3cfe20816  # v1.16
        GIT_SHALLOW    FALSE)
    set(PUGIXML_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(pugixml)
endif()

# 复用已交付的 pk/string（PkString，真正链接的编译期依赖）。
# 守卫式 add_subdirectory：pk/string 自己不 add_subdirectory(pk/test)，
# 所以这里与下面的 pktest 守卫互不冲突（照抄 pk/config/CMakeLists.txt 的模式，
# 该文件顶部注释详细记录了 pk/container/pk/geometry 各自无条件
# add_subdirectory(pk/test) 会撞目标名的坑——pk/xml 不需要 pk/container/
# pk/geometry，不涉及那个坑）。
set(PKSTRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../string)
if (NOT TARGET pkstring)
    add_subdirectory(${PKSTRING_DIR} ${CMAKE_BINARY_DIR}/pkstring)
endif()

add_library(pkxml STATIC
    PkXmlNode.cpp
    PkXmlDocument.cpp
    PkXmlElement.cpp
    PkXmlStreamReader.cpp
    PkXmlStreamWriter.cpp
)
target_include_directories(pkxml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# PUBLIC 而不是 PRIVATE：pugixml.hpp 直接出现在 PkXmlNode.h（公开头文件）里，
# pugixml 的头文件搜索路径必须传递给 pkxml 的使用者（test_pkxml 等），否则
# 只有 pkxml 自己的 .cpp 能找到 <pugixml.hpp>，链接 pkxml 的下游全部编不过。
target_link_libraries(pkxml PUBLIC pkstring pugixml::pugixml)

# 测试用 R-11 的 PK_* harness。只是消费它，一个字节都不改。
set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
if (NOT TARGET pktest)
    add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest)
endif()

pk_test_generate(PKXML_BINDERS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_document.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_element.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_node.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_namespace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stream_writer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_stream_reader.h)

add_executable(test_pkxml
    tests/test_main.cpp
    tests/test_document.cpp
    tests/test_element.cpp
    tests/test_node.cpp
    tests/test_namespace.cpp
    tests/test_stream_writer.cpp
    tests/test_stream_reader.cpp
)
target_include_directories(test_pkxml PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests
    ${PKTEST_DIR}/compat ${CMAKE_CURRENT_SOURCE_DIR}/compat)
target_link_libraries(test_pkxml PRIVATE pkxml pktest)
set_source_files_properties(tests/test_document.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_document.inc")
set_source_files_properties(tests/test_element.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_element.inc")
set_source_files_properties(tests/test_node.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_node.inc")
set_source_files_properties(tests/test_namespace.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_namespace.inc")
set_source_files_properties(tests/test_stream_writer.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_stream_writer.inc")
set_source_files_properties(tests/test_stream_reader.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_test_stream_reader.inc")

# M3（R-07 全分支终审，可选项）：此前没有 add_test()，ctest 会报「No tests
# found」但退出码 0，容易被误读成"没有测试"。挂上去成本极低。
enable_testing()
add_test(NAME test_pkxml COMMAND test_pkxml)
