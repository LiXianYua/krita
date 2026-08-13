set(kritashapemodel_SOURCES
    ../KisReferenceImage.cpp
    ../kis_image_view_converter.cpp
    KisReferenceImagesLayer.cpp
    kis_shape_layer.cc
    kis_shape_layer_canvas.cpp
    kis_shape_selection.cpp
    kis_shape_selection_canvas.cpp
    kis_shape_selection_model.cpp
    kis_take_all_shapes_command.cpp
)

kis_add_library(kritashapemodel SHARED ${kritashapemodel_SOURCES})
generate_export_header(kritashapemodel BASE_NAME kritashapemodel)

target_include_directories(kritashapemodel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(kritashapemodel
    PUBLIC
        kritaflake
        kritaimage
        kritalibbrush
        kritacanvas
)

set_target_properties(kritashapemodel PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION}
    SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritashapemodel ${INSTALL_TARGETS_DEFAULT_ARGS})
