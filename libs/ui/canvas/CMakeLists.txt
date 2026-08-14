set(kritacanvas_LIB_SRCS
    KisCanvasFeedback.cpp
    KisCanvasInvalidation.cpp
    KisCanvasNodeActivation.cpp
    KisColorSamplingCanvas.cpp
    KisMirrorAxisConfig.cpp
    KisCanvasConfig.cpp
    KisCanvasState.cpp
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

kis_add_library(kritacanvas SHARED ${kritacanvas_LIB_SRCS})
generate_export_header(kritacanvas BASE_NAME kritacanvas)

target_include_directories(kritacanvas
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../opengl>
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
