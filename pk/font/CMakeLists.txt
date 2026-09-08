cmake_minimum_required(VERSION 3.16)
project(pkfont CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
enable_testing()

find_package(Threads REQUIRED)
find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)

# This shell embeds pk/image only as a library dependency.  EXCLUDE_FROM_ALL
# keeps its test executables out of the default build, so keep those same
# tests out of this parent project's CTest inventory as well.
set(PKIMAGE_SKIP_TEST_REGISTRATION ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../image
                 ${CMAKE_CURRENT_BINARY_DIR}/pkimage EXCLUDE_FROM_ALL)
unset(PKIMAGE_SKIP_TEST_REGISTRATION)

# 零 Qt：本工程不得出现 find_package(Qt...)。
add_library(pkfont STATIC
    PkFont.cpp
    PkFontDatabase.cpp
    PkFontRasterizer.cpp
)
target_include_directories(pkfont PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkfont PUBLIC pkimage pkstring Freetype::Freetype Fontconfig::Fontconfig)

add_executable(test_pkfont tests/test_font.cpp)
target_link_libraries(test_pkfont PRIVATE pkfont Threads::Threads)
add_test(NAME test_pkfont COMMAND test_pkfont)
