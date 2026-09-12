cmake_minimum_required(VERSION 3.16)
project(pkimage CXX)
include(GNUInstallDirs)

# 独立薄壳工程，不接入 Krita 主构建，也不改根 CMakeLists.txt。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

enable_testing()

find_package(Threads REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/modules")
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(TIFF REQUIRED)
find_package(GIF REQUIRED)
find_package(WebP REQUIRED)

# 零 Qt：本工程不得出现 find_package(Qt...)。
add_library(pkimage STATIC
    PkImage.cpp
)
set_target_properties(pkimage PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(pkimage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(pkimageio SHARED
    PkImageFileDecoder.cpp
    PkPngReader.cpp
    PkPngWriter.cpp
    PkImageFileIo.cpp
    codecs/PkNativeImageCodecs.cpp
    codecs/PkBmpHandler.cpp
    codecs/PkPnmHandler.cpp
    codecs/PkXbmHandler.cpp
    codecs/PkXpmHandler.cpp
    codecs/PkIcoHandler.cpp
)
set_target_properties(pkimageio PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
target_compile_definitions(pkimageio PRIVATE PKIMAGEIO_BUILDING_LIBRARY)
target_include_directories(pkimageio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# 依赖：pk/geometry —— 通过 add_subdirectory 引入（它自带 pktest / pk_test_generate）
# ---------------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../geometry
                  ${CMAKE_CURRENT_BINARY_DIR}/pkgeometry
                  EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# 依赖：pk/string —— R-75：PkImage.h 起用 PkString 做文件路径
# （PkImage(PkString)/load/save），pkimage 的**公共** API 面因此暴露 PkString，
# 故 PUBLIC 链接 pkstring（R-65 教训：主树与薄壳两份依赖声明必须一致）。
# 不需要在这里 add_subdirectory：pk/geometry → pk/test 已经带进 pkstring
# （pk/test/CMakeLists.txt:24 用 CMAKE_BINARY_DIR/pkstring 注册），重复 add
# 会撞「binary directory already used」。
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# 依赖：pk/container —— 只用 include 路径引入（PkArrayData.h 是纯头文件模板），
# 不 add_subdirectory，否则 pktest/selftest_pktest 目标名会与 pk/geometry
# 带出的那份重复定义。
# ---------------------------------------------------------------------------
target_include_directories(pkimage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../container)

target_link_libraries(pkimage
    PUBLIC pkgeometry pkstring)
target_link_libraries(pkimageio
    PUBLIC pkimage
    PRIVATE Threads::Threads PNG::PNG JPEG::JPEG TIFF::TIFF GIF::GIF WebP::webp)

# ---------------------------------------------------------------------------
# 单测：用 pk/test 的 PK_* harness
# ---------------------------------------------------------------------------
pk_test_generate(PKIMAGE_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/image_case.h")

add_executable(test_pkimage
    tests/test_image.cpp
)
# R-75：文件 I/O 用例（PkImage(PkString)/load/save）测的是 pkimageio 里定义的那些
# 符号，所以 test_pkimage 额外链 pkimageio（pkimage 单向依赖 pkimageio 的反向补充，
# 见 pk/image/README.md「落地位置」一节）。`pkimage` 由 pkimageio 的 PUBLIC 链接
# 传递而来，这里不再显式列一次——列了会撞 macOS ld 的
# "ignoring duplicate libraries: 'libpkimage.a'"。
target_link_libraries(test_pkimage PRIVATE pkimageio pktest)
target_compile_definitions(test_pkimage PRIVATE
    PKIMAGE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
target_include_directories(test_pkimage PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../test/compat)
set_source_files_properties(tests/test_image.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_image_case.inc")

if(NOT PKIMAGE_SKIP_TEST_REGISTRATION)
    add_test(NAME test_pkimage COMMAND test_pkimage)
endif()

add_executable(test_pkimage_file_decoder
    tests/test_file_decoder.cpp
)
target_compile_definitions(test_pkimage_file_decoder PRIVATE
    PKIMAGE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
target_link_libraries(test_pkimage_file_decoder PRIVATE pkimageio Threads::Threads)
if(NOT PKIMAGE_SKIP_TEST_REGISTRATION)
    add_test(NAME test_pkimage_file_decoder COMMAND test_pkimage_file_decoder)
endif()

add_library(pkimage_test_registrar SHARED
    tests/file_decoder_registrar.cpp
)
target_link_libraries(pkimage_test_registrar PRIVATE pkimageio)

add_executable(test_pkimage_file_decoder_dso
    tests/test_file_decoder_dso.cpp
)
target_link_libraries(test_pkimage_file_decoder_dso PRIVATE pkimageio ${CMAKE_DL_LIBS})
add_dependencies(test_pkimage_file_decoder_dso pkimage_test_registrar)
if(NOT PKIMAGE_SKIP_TEST_REGISTRATION)
    add_test(NAME test_pkimage_file_decoder_dso
        COMMAND test_pkimage_file_decoder_dso $<TARGET_FILE:pkimage_test_registrar>)
endif()

# Keep the standalone shell's package boundary identical to the aggregate
# target.  The test-only registrar and consumer are installed as a focused
# harness for checking that both DSOs resolve the installed provider.
install(TARGETS pkimageio pkimage_test_registrar test_pkimage_file_decoder_dso
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(NOT PKIMAGE_SKIP_TEST_REGISTRATION)
    add_test(NAME test_pkimage_file_decoder_dso_installed
        COMMAND ${CMAKE_COMMAND}
            -D BUILD_DIR=${CMAKE_BINARY_DIR}
            -D INSTALL_PREFIX=${CMAKE_BINARY_DIR}/installed-decoder
            -D CI_LIBRARY_DIR=$ENV{KDECI_INSTALL_PREFIX}/lib
            -D REGISTRAR_NAME=$<TARGET_FILE_NAME:pkimage_test_registrar>
            -D CONSUMER_NAME=$<TARGET_FILE_NAME:test_pkimage_file_decoder_dso>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_installed_decoder.cmake)
endif()
