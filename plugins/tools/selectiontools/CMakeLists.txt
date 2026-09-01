set(kritaselectiontools_SOURCES
    selection_tools.cc
    kis_tool_select_rectangular.cc
    kis_tool_select_polygonal.cc
    kis_tool_select_elliptical.cc
    kis_tool_select_contiguous.cc
    kis_tool_select_outline.cc
    kis_tool_select_path.cc
    kis_tool_select_similar.cc
    kis_selection_modifier_mapper.cc
    KisMagneticWorker.cc
    KisToolSelectMagnetic.cc
)

qt_add_resources(kritaselectiontools_SOURCES selectiontools.qrc)

kis_add_library(kritaselectiontools MODULE ${kritaselectiontools_SOURCES})

set_target_properties(kritaselectiontools PROPERTIES AUTOMOC OFF)
target_include_directories(kritaselectiontools PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/string)

generate_export_header(kritaselectiontools BASE_NAME kritaselectiontools)

target_link_libraries(kritaselectiontools
    kritacanvas
    kritaflake
    kritashapemodel
    kritaimage
    kritaresources
    KF${KF_MAJOR}::ConfigCore
    Boost::boost
    pkcolor
    pkcontainer
    pkgeometry
    pkpointer
    pkrender
    pksignal
    pkstring)

install(TARGETS kritaselectiontools  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install( FILES
        KisToolSelectPolygonal.action
        KisToolSelectElliptical.action
        KisToolSelectSimilar.action
        KisToolSelectContiguous.action
        KisToolSelectRectangular.action
        KisToolSelectOutline.action
        KisToolSelectPath.action
        KisToolSelectMagnetic.action
    DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions
)
