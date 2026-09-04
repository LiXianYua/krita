cmake_minimum_required(VERSION 3.16)
project(pkdom CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 零 Qt：本工程不得出现 find_package(Qt...)。判据③ 靠 nm -u 自证。
#
# pugixml：与 pk/xml 同一份依赖获取方式（装了用系统、没装 FetchContent 下载，
# tag+hash 钉死，不 vendor 进 fork）。R-50 锁只禁动 pk/xml 的「文件」，这里只用
# pugixml 库本身，不碰 pk/xml 源码。
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

# 复用已交付的 pk/test harness（PK_TEST 宏），与 pk/xml 同款做法。
set(PKTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../test)
if (NOT TARGET pktest)
    add_subdirectory(${PKTEST_DIR} ${CMAKE_BINARY_DIR}/pktest)
endif()

add_library(pkdom STATIC
    PkDom.cpp
    PkDomElement.cpp
    PkDomDocument.cpp
)
target_include_directories(pkdom PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# PUBLIC：pugixml.hpp 出现在 PkDomDocument.cpp（及将来公开头）里，下游用 pkdom
# 时必须能找到 <pugixml.hpp>。
target_link_libraries(pkdom PUBLIC pugixml::pugixml)

add_executable(pkdom_smoke tests/smoke_main.cpp)
target_include_directories(pkdom_smoke PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(pkdom_smoke PRIVATE pkdom)

enable_testing()
add_test(NAME pkdom_smoke COMMAND pkdom_smoke)
