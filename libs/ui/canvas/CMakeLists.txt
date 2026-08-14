set(kritacanvas_LIB_SRCS
    KisCanvasFeedback.cpp
    KisCanvasInvalidation.cpp
    KisCanvasNodeActivation.cpp
    KisCanvasToolServices.cpp
    KisColorSamplingCanvas.cpp
    KisReferenceImageToolServices.cpp
    KisStandardBrushSizes.cpp
    KisMirrorAxisConfig.cpp
    KisCanvasConfig.cpp
    KisCanvasState.cpp
    ../tool/kis_tool.cc
    ../tool/kis_tool_freehand.cc
    ../tool/kis_speed_smoother.cpp
    ../tool/kis_painting_information_builder.cpp
    ../tool/kis_stabilized_events_sampler.cpp
    ../tool/kis_tool_freehand_helper.cpp
    ../tool/kis_tool_multihand_helper.cpp
    ../tool/kis_tool_paint.cc
    ../tool/kis_smoothing_options.cpp
    ../tool/KisStabilizerDelayedPaintHelper.cpp
    ../tool/strokes/kis_color_sampler_stroke_strategy.cpp
    ../tool/KisAsyncColorSamplerHelper.cpp
    ../tool/KisToolPaintFactoryBase.cpp
    ../tool/KisSelectionToolFactoryBase.cpp
    ../tool/kis_rectangle_constraint_widget.cpp
    ../tool/kis_shape_tool_helper.cpp
    ../tool/kis_tool_shape.cc
    ../tool/kis_tool_ellipse_base.cpp
    ../tool/kis_tool_rectangle_base.cpp
    ../tool/kis_tool_polyline_base.cpp
    ../tool/KisToolOutlineBase.cpp
    ../kis_aspect_ratio_locker.cpp
    ../widgets/kis_selection_options.cc
    ../widgets/kis_color_label_selector_widget.cpp
    ../widgets/kis_color_label_button.cpp
    ../tool/kis_selection_tool_config_widget_helper.cpp
    ../tool/kis_delegated_tool_policies.cpp
    ../tool/strokes/move_stroke_strategy.cpp
    ../tool/strokes/KisNodeSelectionRecipe.cpp
    ../kis_gui_context_command.cpp
    ../kis_gui_context_command_p.cpp
    ../processing/fill_processing_visitor.cpp
    ../processing/KisEncloseAndFillProcessingVisitor.cpp
    ../tool/KisToolChangesTracker.cpp
    ../tool/KisToolChangesTrackerData.cpp
    KisDisplayConfig.cpp
    kis_display_color_converter.cpp
    kis_grid_config.cpp
    kis_guides_config.cpp
    kis_coordinates_converter.cpp
    kis_display_filter.cpp
    kis_exposure_gamma_correction_interface.cpp
    kis_image_patch.cpp
    kis_image_pyramid.cpp
    kis_prescaled_projection.cpp
    kis_projection_backend.cpp
    kis_ui_types.h
    kis_update_info.cpp
    ../opengl/KisOpenGLUpdateInfoBuilder.cpp
    ../opengl/kis_texture_tile_info_pool.cpp
)

ki18n_wrap_ui(kritacanvas_LIB_SRCS
    ../forms/wdggeometryoptions.ui
    ../forms/wdgrectangleconstraints.ui
)

kis_add_library(kritacanvas SHARED ${kritacanvas_LIB_SRCS})
generate_export_header(kritacanvas BASE_NAME kritacanvas)

target_include_directories(kritacanvas
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../tool>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../opengl>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../widgets>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../processing>
)

target_link_libraries(kritacanvas
    PUBLIC
        Boost::boost
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Widgets
        kritacolor
        kritaflake
        kritaglobal
        kritaimage
        kritainput
        kritalibpaintop
        kritawidgets
    PRIVATE
        KF${KF_MAJOR}::ConfigCore
)

set_target_properties(kritacanvas PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION}
    SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritacanvas ${INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
