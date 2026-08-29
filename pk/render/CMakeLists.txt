cmake_minimum_required(VERSION 3.16)
project(pkrender CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../geometry ${CMAKE_CURRENT_BINARY_DIR}/pkgeometry EXCLUDE_FROM_ALL)
add_library(pkrender STATIC PkPainter.cpp ../image/PkImage.cpp ../color/PkColor.cpp
    ../global/PkGlobal.cpp ../string/PkStringCodec.cpp ../string/PkString_core.cpp
    ../string/PkString_query.cpp ../string/PkString_format.cpp)
target_include_directories(pkrender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ../image ../container ../color ../namespace ../global)
target_link_libraries(pkrender PUBLIC pkgeometry)
pk_test_generate(PKRENDER_GENERATED_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/painter_case.h")
add_executable(test_pkrender tests/test_painter.cpp)
target_link_libraries(test_pkrender PRIVATE pkrender pktest)
target_include_directories(test_pkrender PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../test
    ${CMAKE_CURRENT_BINARY_DIR})
set_source_files_properties(tests/test_painter.cpp PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_painter_case.inc")
add_library(retained_tools_compile OBJECT tests/retained_tools_compile.cpp)
target_link_libraries(retained_tools_compile PRIVATE pkrender)
