add_subdirectory(tests)

set(kritadefaulttools_static_SRCS
    kis_tool_colorsampler.cc
    kis_tool_brush.cc
    kis_tool_line.cc
    kis_tool_line_helper.cpp
    kis_tool_fill.cc
    kis_tool_rectangle.cc
    kis_tool_ellipse.cc
    kis_tool_gradient.cc
    kis_tool_measure.cc
    kis_tool_path.cc
    kis_tool_move.cc
    strokes/move_selection_stroke_strategy.cpp
    KisMoveBoundsCalculationJob.cpp
    kis_tool_multihand.cpp
    kis_tool_pencil.cc
    kis_tool_pan.cpp
)

kis_add_library(kritadefaulttools_static STATIC ${kritadefaulttools_static_SRCS})

set_target_properties(kritadefaulttools_static PROPERTIES AUTOMOC OFF)
target_include_directories(kritadefaulttools_static PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/xml)

target_link_libraries(kritadefaulttools_static 
    PUBLIC
        kritacanvas
        kritaflake
        kritashapemodel
        kritaimage
        Boost::boost
        pkcolor
        pkcontainer
        pkgeometry
        pkpointer
        pkrender
        pksignal
        pkstring
        pkvariant
        pkxml
)

set(kritadefaulttools_SOURCES
    default_tools.cc
)

qt_add_resources(kritadefaulttools_SOURCES defaulttools.qrc )

kis_add_library(kritadefaulttools MODULE ${kritadefaulttools_SOURCES})

set_target_properties(kritadefaulttools PROPERTIES AUTOMOC OFF)
target_include_directories(kritadefaulttools PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/xml)

generate_export_header(kritadefaulttools BASE_NAME kritadefaulttools)

target_link_libraries(kritadefaulttools kritadefaulttools_static)

install(TARGETS kritadefaulttools  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES
    KisToolPath.action
    KisToolPencil.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
