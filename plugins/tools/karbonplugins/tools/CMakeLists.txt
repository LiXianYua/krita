set(karbon_tools_SOURCES
   KarbonToolsPlugin.cpp
   CalligraphyTool/KarbonCalligraphyTool.cpp
   CalligraphyTool/KarbonCalligraphyToolFactory.cpp
   CalligraphyTool/KarbonCalligraphicShape.cpp
   CalligraphyTool/KarbonCalligraphicShapeFactory.cpp
   CalligraphyTool/KarbonSimplifyPath.cpp
)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/22-actions-calligraphy.png" KARBON_CALLIGRAPHY_PNG_HEX HEX)
file(SIZE "${CMAKE_CURRENT_SOURCE_DIR}/22-actions-calligraphy.png" KARBON_CALLIGRAPHY_PNG_SIZE)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," KARBON_CALLIGRAPHY_PNG_BYTES
    "${KARBON_CALLIGRAPHY_PNG_HEX}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/qrc_karbontools.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/qrc_karbontools.cpp"
    @ONLY)
list(APPEND karbon_tools_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/qrc_karbontools.cpp")

kis_add_library(krita_karbontools STATIC ${karbon_tools_SOURCES})

set_target_properties(krita_karbontools PROPERTIES AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(krita_karbontools PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/xml)

target_link_libraries(krita_karbontools
    kritacanvas pkcolor pkcontainer pkgeometry pkpointer pkrender pkstring pkxml)

install(TARGETS krita_karbontools DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
