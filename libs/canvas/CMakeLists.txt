set(kritacanvas_LIB_SRCS
    KisCanvasFeedback.cpp
    KisCanvasInvalidation.cpp
    KisCanvasNodeActivation.cpp
    KisCanvasToolServices.cpp
    KisColorSamplingCanvas.cpp
    KisReferenceImageToolServices.cpp
    KisStandardBrushSizes.cpp
    KisMirrorAxisConfig.cpp
    KisMultiSurfaceStateManager.cpp
    KisCanvasConfig.cpp
    KisCanvasState.cpp
    tool/kis_tool.cc
    tool/kis_tool_freehand.cc
    tool/kis_speed_smoother.cpp
    tool/kis_painting_information_builder.cpp
    tool/kis_stabilized_events_sampler.cpp
    tool/kis_tool_freehand_helper.cpp
    tool/kis_tool_multihand_helper.cpp
    tool/kis_tool_paint.cc
    tool/kis_smoothing_options.cpp
    tool/KisStabilizerDelayedPaintHelper.cpp
    tool/strokes/kis_color_sampler_stroke_strategy.cpp
    tool/KisAsyncColorSamplerHelper.cpp
    tool/KisToolPaintFactoryBase.cpp
    tool/KisSelectionToolFactoryBase.cpp
    tool/kis_shape_tool_helper.cpp
    tool/kis_tool_shape.cc
    tool/kis_tool_ellipse_base.cpp
    tool/kis_tool_rectangle_base.cpp
    tool/kis_tool_polyline_base.cpp
    tool/KisToolOutlineBase.cpp
    kis_aspect_ratio_locker.cpp
    widgets/kis_selection_options.cc
    tool/kis_selection_tool_config_widget_helper.cpp
    tool/kis_delegated_tool_policies.cpp
    tool/strokes/move_stroke_strategy.cpp
    tool/strokes/KisNodeSelectionRecipe.cpp
    kis_gui_context_command.cpp
    kis_gui_context_command_p.cpp
    processing/fill_processing_visitor.cpp
    processing/KisEncloseAndFillProcessingVisitor.cpp
    tool/KisToolChangesTracker.cpp
    tool/KisToolChangesTrackerData.cpp
    KisDisplayConfig.cpp
    kis_display_color_converter.cpp
    kis_grid_config.cpp
    kis_guides_config.cpp
    kis_coordinates_converter.cpp
    KoZoomHandler.cpp
    kis_display_filter.cpp
    kis_exposure_gamma_correction_interface.cpp
    kis_image_patch.cpp
    kis_image_pyramid.cpp
    kis_prescaled_projection.cpp
    kis_projection_backend.cpp
    kis_ui_types.h
    kis_update_info.cpp
    opengl/KisOpenGLPolicy.cpp
    KisCanvasSurfaceColorSpacePolicy.cpp
    opengl/KisOpenGLUpdateInfoBuilder.cpp
    opengl/kis_texture_tile_info_pool.cpp
)

kis_add_library(kritacanvas SHARED ${kritacanvas_LIB_SRCS})
generate_export_header(kritacanvas BASE_NAME kritacanvas)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(kritacanvas_SIGNAL_HEADERS
    KisCanvasToolServices.h
    kis_aspect_ratio_locker.h
    kis_display_color_converter.h
    opengl/kis_texture_tile_info_pool.h
    tool/KisAsyncColorSamplerHelper.h
    tool/kis_smoothing_options.h
    tool/kis_tool.h
    tool/kis_tool_freehand_helper.h
    tool/kis_tool_paint.h
    tool/kis_tool_rectangle_base.h
    tool/strokes/kis_color_sampler_stroke_strategy.h
    tool/kis_selection_tool_config_widget_helper.h
    widgets/kis_selection_options.h
)
set(kritacanvas_SIGNAL_SRCS)
foreach(signal_header IN LISTS kritacanvas_SIGNAL_HEADERS)
    get_filename_component(signal_name "${signal_header}" NAME_WE)
    set(signal_source "${CMAKE_CURRENT_BINARY_DIR}/${signal_name}_pk_signals.cpp")
    add_custom_command(
        OUTPUT "${signal_source}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
                -o "${signal_source}"
        DEPENDS
                "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
        COMMENT "pk_signal_moc: generating ${signal_name} signal definitions"
        VERBATIM
    )
    list(APPEND kritacanvas_SIGNAL_SRCS "${signal_source}")
endforeach()
set_source_files_properties(${kritacanvas_SIGNAL_SRCS} PROPERTIES SKIP_AUTOMOC ON)
target_sources(kritacanvas PRIVATE ${kritacanvas_SIGNAL_SRCS})

target_include_directories(kritacanvas
    BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/flake/flake/noqt-compat
        ${CMAKE_SOURCE_DIR}/pk/string/compat
        ${CMAKE_SOURCE_DIR}/pk/container/compat
        ${CMAKE_SOURCE_DIR}/pk/variant/compat
        ${CMAKE_SOURCE_DIR}/pk/port/compat
        ${CMAKE_SOURCE_DIR}/pk/pointer/compat
        ${CMAKE_SOURCE_DIR}/pk/global/compat
        ${CMAKE_SOURCE_DIR}/pk/geometry/compat
        ${CMAKE_SOURCE_DIR}/pk/log/compat
        ${CMAKE_SOURCE_DIR}/pk/concurrent/compat
        ${CMAKE_SOURCE_DIR}/pk/image/compat
        ${CMAKE_SOURCE_DIR}/pk/time/compat
        ${CMAKE_SOURCE_DIR}/pk/config/compat
        ${CMAKE_SOURCE_DIR}/pk/flags/compat
        ${CMAKE_SOURCE_DIR}/pk/color/compat
        ${CMAKE_SOURCE_DIR}/pk/signal/compat
        ${CMAKE_SOURCE_DIR}/pk/sql/compat
        ${CMAKE_SOURCE_DIR}/pk/xml/compat
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tool>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/opengl>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/widgets>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/processing>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../surfacecolormanagementapi>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../surfacecolormanagementapi>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/string>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/container>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/geometry>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/color>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/image>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/render>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/pointer>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/config>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/global>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/xml>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/signal>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/concurrent>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/time>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/log>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/input>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/input>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/libs/input>
)

target_link_libraries(kritacanvas
    PUBLIC
        Boost::boost
        kritaimage_noqt_compile_interface
        kritalibpaintop_noqt_compile_interface
        pkstring
        pkcontainer
        pkgeometry
        pkcolor
        pkimage
        pkrender
        pkpointer
        pkconfig
        pkglobal
        pkxml
        pksignal
        pkconcurrent
        pktime
        pklog
        pkinput
        $<LINK_ONLY:Qt${QT_MAJOR_VERSION}::Core>
        $<LINK_ONLY:Qt${QT_MAJOR_VERSION}::Gui>
        $<LINK_ONLY:Qt${QT_MAJOR_VERSION}::Widgets>
        $<LINK_ONLY:kritacolor>
        $<LINK_ONLY:kritaflake>
        $<LINK_ONLY:kritaglobal>
        $<LINK_ONLY:kritaimage>
        $<LINK_ONLY:kritainput>
        $<LINK_ONLY:kritasurfacecolormanagementapi>
    PRIVATE
        $<LINK_ONLY:KF${KF_MAJOR}::ConfigCore>
)

set_target_properties(kritacanvas PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION}
    SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritacanvas ${INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
