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
    kis_selection_modifier_mapper_notifier.cpp
    KisMagneticWorker.cc
    KisToolSelectMagnetic.cc
    selection_tool_cursor.cc
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# This bridge is intentionally Pk-only. The surrounding legacy target still
# inherits QT_CORE_LIB through unrelated GUI sources, which would otherwise
# disable the compat/QObject declarations used by KisConfigNotifier.
set_source_files_properties(kis_selection_modifier_mapper_notifier.cpp PROPERTIES
    COMPILE_OPTIONS "-UQT_CORE_LIB")

kis_add_library(kritaselectiontools STATIC ${kritaselectiontools_SOURCES})

set_target_properties(kritaselectiontools PROPERTIES AUTOMOC OFF)
target_include_directories(kritaselectiontools PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/config
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/input
    ${CMAKE_SOURCE_DIR}/pk/namespace
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
    Boost::boost
    pkcolor
    pkcontainer
    pkgeometry
    pkconfig
    pkimage
    pknamespace
    pkpointer
    pkrender
    pksignal
    pkstring)

install(TARGETS kritaselectiontools  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

# Native hosts consume the cursor payload descriptors; do not generate a Qt
# RCC translation unit for this plugin.
install(FILES
        tool_contiguous_selection_cursor.png
        tool_contiguous_selection_cursor_add.png
        tool_contiguous_selection_cursor_inter.png
        tool_contiguous_selection_cursor_sub.png
        tool_contiguous_selection_cursor_symdiff.png
        tool_elliptical_selection_cursor.png
        tool_elliptical_selection_cursor_add.png
        tool_elliptical_selection_cursor_inter.png
        tool_elliptical_selection_cursor_sub.png
        tool_elliptical_selection_cursor_symdiff.png
        tool_elliptical_selection_enclose_eraser_cursor.png
        tool_eraser_selection_cursor.png
        tool_magnetic_selection_cursor.png
        tool_magnetic_selection_cursor_add.png
        tool_magnetic_selection_cursor_inter.png
        tool_magnetic_selection_cursor_sub.png
        tool_magnetic_selection_cursor_symdiff.png
        tool_outline_selection_cursor.png
        tool_outline_selection_cursor_add.png
        tool_outline_selection_cursor_inter.png
        tool_outline_selection_cursor_sub.png
        tool_outline_selection_cursor_symdiff.png
        tool_outline_selection_enclose_eraser_cursor.png
        tool_polygonal_selection_cursor.png
        tool_polygonal_selection_cursor_add.png
        tool_polygonal_selection_cursor_inter.png
        tool_polygonal_selection_cursor_sub.png
        tool_polygonal_selection_cursor_symdiff.png
        tool_polygonal_selection_enclose_eraser_cursor.png
        tool_rectangular_selection_cursor.png
        tool_rectangular_selection_cursor_add.png
        tool_rectangular_selection_cursor_inter.png
        tool_rectangular_selection_cursor_sub.png
        tool_rectangular_selection_cursor_symdiff.png
        tool_rectangular_selection_enclose_eraser_cursor.png
        tool_similar_selection_cursor.png
        tool_similar_selection_cursor_add.png
        tool_similar_selection_cursor_inter.png
        tool_similar_selection_cursor_sub.png
        tool_similar_selection_cursor_symdiff.png
        DESTINATION ${KRITA_PLUGIN_INSTALL_DIR}/selection-cursors)

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
