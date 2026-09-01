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

# 测试用 R-11 的 PK_* harness。pktest 会带出 pkstring 目标，所以要在
# 下方消费 pkstring 之前先建立这条边，避免同一源目录/二进制目录被
# add_subdirectory 两次。
set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
if (NOT TARGET pktest)
    add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest)
endif()

# 复用已交付的 pk/string（PkString，真正链接的编译期依赖）。
set(PKSTRING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../string)
if (NOT TARGET pkstring)
    add_subdirectory(${PKSTRING_DIR} ${CMAKE_BINARY_DIR}/pkstring)
endif()

set(PKCONTAINER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../container)
if (NOT TARGET pkcontainer)
    set(PKCONTAINER_LIBRARY_ONLY ON)
    add_subdirectory(${PKCONTAINER_DIR} ${CMAKE_CURRENT_BINARY_DIR}/pkcontainer EXCLUDE_FROM_ALL)
    unset(PKCONTAINER_LIBRARY_ONLY)
endif()

# R-25 Task 2：setContent(PkStream*, ...) 两个重载要真的链接 PkStream::read()
# 这个非 inline 符号（定义在 pk/port/PkStream.cpp）——只拿头文件目录不够。
# 直接把 .cpp 加进本库、不 add_subdirectory(../port)：那会把 pk/port 自己的
# project()/test_pkport/pkzip/FetchContent(minizip-ng) 一整条依赖链拖进来，
# 与本库无关。参照 pk/port/CMakeLists.txt 自己对 pk/string 的处理选型（同一
# 文件"直接加 .cpp、不 add_subdirectory"的注释），采用同样的做法——
# `PkStream.cpp` 的值类型依赖由上方 pkcontainer 目标供给；这里仍仅内联
# stream 实现，不拉入 port/zip 适配层。见 docs/superpowers/plans/R-25.md 设计①-a。
set(PKPORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../port)

add_library(pkxml STATIC
    PkXmlNode.cpp
    PkXmlDocument.cpp
    PkXmlElement.cpp
    PkXmlOffsetUtil.cpp
    PkXmlStreamReader.cpp
    PkXmlStreamWriter.cpp
    ${PKPORT_DIR}/PkStream.cpp
)
target_include_directories(pkxml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${PKPORT_DIR})
# PUBLIC 而不是 PRIVATE：pugixml.hpp 直接出现在 PkXmlNode.h（公开头文件）里，
# pugixml 的头文件搜索路径必须传递给 pkxml 的使用者（test_pkxml 等），否则
# 只有 pkxml 自己的 .cpp 能找到 <pugixml.hpp>，链接 pkxml 的下游全部编不过。
# 同理 PkStream.h（PUBLIC 头文件 PkXmlDocument.h 里 `setContent(PkStream*...)`
# 要用到）也要传递给下游。
target_link_libraries(pkxml PUBLIC pkcontainer pkstring pugixml::pugixml)

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
