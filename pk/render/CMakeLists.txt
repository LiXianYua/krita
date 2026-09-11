cmake_minimum_required(VERSION 3.16)
project(pkrender C CXX)
set(CMAKE_CXX_STANDARD 17)
enable_testing()
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/modules)
include(FeatureSummary)
find_package(libunibreak 5.0 REQUIRED)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../font ${CMAKE_CURRENT_BINARY_DIR}/pkfont EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../xml ${CMAKE_CURRENT_BINARY_DIR}/pkxml EXCLUDE_FROM_ALL)
add_library(pkrender STATIC PkPainter.cpp PkStrokeOutline.cpp PkSvgRasterizer.cpp ../color/PkColor.cpp
    ../global/PkGlobal.cpp
    ../../libs/flake/PkImageRasterBackend.cpp ../../libs/flake/PkGrayRaster.c
    ../../libs/flake/PkAliasedRasterizer.cpp ../../libs/flake/PkCosmeticStroker.cpp
    ../../libs/flake/svg/parsers/SvgTransformParser.cpp
    ../../libs/flake/svg/SvgCssHelper.cpp)
target_include_directories(pkrender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../image ../container ../color ../namespace ../global ../font)
target_include_directories(pkrender PUBLIC ../../libs/flake)
target_include_directories(pkrender PUBLIC ../../libs/flake/svg/parsers ../../libs/flake/svg ../log)
target_link_libraries(pkrender PUBLIC pkgeometry pkfont pkxml pkimageio libunibreak::libunibreak)
pk_test_generate(PKRENDER_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/painter_case.h")
add_executable(test_pkrender tests/test_painter.cpp)
target_link_libraries(test_pkrender PRIVATE pkrender pktest)
target_include_directories(test_pkrender PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../test
    ${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(tests/test_painter.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_painter_case.inc")
add_executable(test_pksvg_rasterizer tests/test_svg_rasterizer.cpp)
target_link_libraries(test_pksvg_rasterizer PRIVATE pkrender)
add_test(NAME test_pksvg_rasterizer COMMAND test_pksvg_rasterizer)

# R-51: 形状原语（椭圆/多边形）对拍。
#   shape_primitive_oracle_pk —— Pk 侧 oracle 输出体（Qt-free，正常进本工程构建）。
#   test_shape_primitive      —— ctest 目标，期望值来自 oracle/shape_primitive_golden.txt，
#                                那份文件由 oracle/run_shape_primitive.sh 从**真 Qt 侧**产出。
# Qt 侧 oracle 仍然只由脚本手工编译：本工程不得 find_package(Qt...)，也不得链 Qt。
add_executable(shape_primitive_oracle_pk oracle/shape_primitive_oracle.cpp)
target_link_libraries(shape_primitive_oracle_pk PRIVATE pkrender)
target_include_directories(shape_primitive_oracle_pk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/oracle)

add_executable(test_shape_primitive tests/test_shape_primitive.cpp)
target_link_libraries(test_shape_primitive PRIVATE pkrender)
target_include_directories(test_shape_primitive PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/oracle)
add_test(NAME test_shape_primitive COMMAND test_shape_primitive)
set_tests_properties(test_shape_primitive PROPERTIES
    ENVIRONMENT "PK_RENDER_SHAPE_PRIMITIVE_GOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/oracle/shape_primitive_golden.txt")

# R-52: 两处 blur 滤镜内核构造（AA 1px cosmetic 直线 / AA 凸多边形填充 + translate）对拍。
#   blur_kernel_oracle_pk —— Pk 侧 oracle 输出体（Qt-free，正常进本工程构建）。
#   test_blur_kernel      —— ctest 目标，期望值来自 oracle/blur_kernel_golden.txt，
#                            那份文件由 oracle/run_blur_kernel.sh 从**真 Qt 侧**产出。
# Qt 侧 oracle 仍然只由脚本手工编译：本工程不得 find_package(Qt...)，也不得链 Qt。
add_executable(blur_kernel_oracle_pk oracle/blur_kernel_oracle.cpp)
target_link_libraries(blur_kernel_oracle_pk PRIVATE pkrender)
target_include_directories(blur_kernel_oracle_pk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/oracle)

add_executable(test_blur_kernel tests/test_blur_kernel.cpp)
target_link_libraries(test_blur_kernel PRIVATE pkrender)
target_include_directories(test_blur_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/oracle)
add_test(NAME test_blur_kernel COMMAND test_blur_kernel)
set_tests_properties(test_blur_kernel PROPERTIES
    ENVIRONMENT "PK_RENDER_BLUR_KERNEL_GOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/oracle/blur_kernel_golden.txt")

# R-55: 文字（setFont / drawText@Point）对拍。
#   text_oracle_pk —— Pk 侧 oracle 输出体（Qt-free，正常进本工程构建）。
#   test_text      —— ctest 目标，期望值来自 oracle/text_golden.txt。
#                     ⚠ 本机那份 golden 由 run_text.sh 在**无 FreeType 宿主**时从
#                     **Pk 侧**产出（出生证明如实写 Pk，见 plan §6 第 6 条）——
#                     本机它守的是「Pk 侧自注入以来没漂」，不是「与 Qt 逐像素相等」。
#                     有 FreeType 的宿主跑一次 run_text.sh 会换成 Qt 出生证明。
# Qt 侧 oracle 仍然只由脚本手工编译：本工程不得 find_package(Qt...)，也不得链 Qt。
add_executable(text_oracle_pk oracle/text_oracle.cpp)
target_link_libraries(text_oracle_pk PRIVATE pkrender)
target_include_directories(text_oracle_pk PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/oracle)

add_executable(test_text tests/test_text.cpp)
target_link_libraries(test_text PRIVATE pkrender)
target_include_directories(test_text PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/oracle)

# 文字用例的期望值与 **fontconfig 的配置**有关（plan §6 第 5 条）：不给
# FONTCONFIG_PATH，fontconfig 退回宿主默认配置，字体回退与字形栅格随之改变，
# 同一个二进制在「没说清环境」的 shell 里就会假红——实测过一次（source 了 env
# 但没有 export FONTCONFIG_PATH 时 ctest 红、导出了就绿）。ctest **不继承**我们
# source 过的 env 脚本，所以把值烘进测试环境：优先用现成的 FONTCONFIG_PATH，
# 否则从 CMAKE_PREFIX_PATH 推 <prefix>/etc/fonts。
set(PK_RENDER_FONTCONFIG_PATH "$ENV{FONTCONFIG_PATH}")
if(NOT PK_RENDER_FONTCONFIG_PATH)
    foreach(_pk_prefix IN LISTS CMAKE_PREFIX_PATH)
        if(EXISTS "${_pk_prefix}/etc/fonts")
            set(PK_RENDER_FONTCONFIG_PATH "${_pk_prefix}/etc/fonts")
            break()
        endif()
    endforeach()
endif()

set(PK_RENDER_TEXT_ENV "PK_RENDER_TEXT_GOLDEN=${CMAKE_CURRENT_SOURCE_DIR}/oracle/text_golden.txt")
if(PK_RENDER_FONTCONFIG_PATH)
    list(APPEND PK_RENDER_TEXT_ENV "FONTCONFIG_PATH=${PK_RENDER_FONTCONFIG_PATH}")
else()
    message(WARNING "pk/render: no <prefix>/etc/fonts under CMAKE_PREFIX_PATH and no "
                    "FONTCONFIG_PATH; test_text will fall back to the host fontconfig config "
                    "and may fail for environment reasons (plan §6 item 5)")
endif()

add_test(NAME test_text COMMAND test_text)
set_tests_properties(test_text PROPERTIES ENVIRONMENT "${PK_RENDER_TEXT_ENV}")

add_library(retained_tools_compile OBJECT tests/retained_tools_compile.cpp)
target_link_libraries(retained_tools_compile PRIVATE pkrender)
