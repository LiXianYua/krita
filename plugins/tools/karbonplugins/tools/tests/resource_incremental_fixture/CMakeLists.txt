cmake_minimum_required(VERSION 3.16)
project(KarbonToolsResourceIncrementalFixture LANGUAGES C CXX)

include("${KARBON_RESOURCE_SOURCE_DIR}/KarbonToolsResource.cmake")
karbon_embed_calligraphy_resource(
    "${CMAKE_CURRENT_SOURCE_DIR}/calligraphy.png"
    "${KARBON_RESOURCE_SOURCE_DIR}/qrc_karbontools.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/qrc_karbontools.cpp")

add_executable(karbon_resource_probe
    main.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/qrc_karbontools.cpp")
target_compile_features(karbon_resource_probe PRIVATE cxx_std_17)
target_include_directories(karbon_resource_probe PRIVATE "${KARBON_RESOURCE_SOURCE_DIR}")
