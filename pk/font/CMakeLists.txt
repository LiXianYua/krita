cmake_minimum_required(VERSION 3.16)
project(pkfont C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
enable_testing()

find_package(Threads REQUIRED)
find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/modules)
include(FeatureSummary)
find_package(HarfBuzz REQUIRED)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty_vendor/raqm
                 ${CMAKE_CURRENT_BINARY_DIR}/raqm)

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
target_link_libraries(pkfont PUBLIC pkimage pkstring Freetype::Freetype Fontconfig::Fontconfig Raqm::Raqm HarfBuzz::HarfBuzz)

add_executable(test_pkfont tests/test_font.cpp)
target_link_libraries(test_pkfont PRIVATE pkfont Threads::Threads)
add_test(NAME test_pkfont COMMAND test_pkfont)

# R-76：把 fontconfig 配置烘进 ctest 环境 —— 与 pk/render 给 test_text 做的是同一件事、
# 同一个理由。本测试 pin 的是 "DejaVu Sans" 24px 的逐像素值（:80 的 hash），它经
# fontconfig 解析字体文件：宿主默认配置下那句 `Cannot load default config file` 在本机
# 是日志噪音（解析到同一个文件），但换个宿主就可能解析到别的字体 —— 那时它就是**环境
# 假红**，而 R-76 已把它拉进收尾入口，红的是整条入口。
# 探测逻辑只有一份（父工程的 PK_RENDER_FONTCONFIG_PATH）；pk/font 单独 configure 时它
# 为空，此时行为与加这段之前**逐字相同**。
if(PK_RENDER_FONTCONFIG_PATH)
    set_tests_properties(test_pkfont PROPERTIES
        ENVIRONMENT "FONTCONFIG_PATH=${PK_RENDER_FONTCONFIG_PATH}")
endif()
