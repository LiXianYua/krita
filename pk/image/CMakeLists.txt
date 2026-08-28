cmake_minimum_required(VERSION 3.16)
project(pkimage CXX)

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
    codecs/PkNativeImageCodecs.cpp
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
# 依赖：pk/container —— 只用 include 路径引入（PkArrayData.h 是纯头文件模板），
# 不 add_subdirectory，否则 pktest/selftest_pktest 目标名会与 pk/geometry
# 带出的那份重复定义。
# ---------------------------------------------------------------------------
target_include_directories(pkimage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../container)

target_link_libraries(pkimage
    PUBLIC pkgeometry)
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
target_link_libraries(test_pkimage PRIVATE pkimage pktest)
target_include_directories(test_pkimage PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../test/compat)
set_source_files_properties(tests/test_image.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_image_case.inc")

add_test(NAME test_pkimage COMMAND test_pkimage)

add_executable(test_pkimage_file_decoder
    tests/test_file_decoder.cpp
)
target_compile_definitions(test_pkimage_file_decoder PRIVATE
    PKIMAGE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
target_link_libraries(test_pkimage_file_decoder PRIVATE pkimageio Threads::Threads)
add_test(NAME test_pkimage_file_decoder COMMAND test_pkimage_file_decoder)

add_library(pkimage_test_registrar SHARED
    tests/file_decoder_registrar.cpp
)
target_link_libraries(pkimage_test_registrar PRIVATE pkimageio)

add_executable(test_pkimage_file_decoder_dso
    tests/test_file_decoder_dso.cpp
)
target_link_libraries(test_pkimage_file_decoder_dso PRIVATE pkimageio ${CMAKE_DL_LIBS})
add_dependencies(test_pkimage_file_decoder_dso pkimage_test_registrar)
add_test(NAME test_pkimage_file_decoder_dso
    COMMAND test_pkimage_file_decoder_dso $<TARGET_FILE:pkimage_test_registrar>)
