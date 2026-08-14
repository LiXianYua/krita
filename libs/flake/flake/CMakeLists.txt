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
    kis_node_dummies_graph.cpp
    kis_dummies_facade_base.cpp
    kis_dummies_facade.cpp
    kis_node_shapes_graph.cpp
    kis_node_shape.cpp
    kis_shape_controller.cpp
    kis_shape_controller_ui_adapter.cpp
    ../tool/kis_selection_tool_helper.cpp
)

kis_add_library(kritashapemodel SHARED ${kritashapemodel_SOURCES})
generate_export_header(kritashapemodel BASE_NAME kritashapemodel)

target_include_directories(kritashapemodel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../tool>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(kritashapemodel
    PUBLIC
        kritaflake
        kritaimage
        kritalibbrush
        kritacanvas

    PRIVATE
        KF${KF_MAJOR}::I18n
)

set_target_properties(kritashapemodel PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION}
    SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritashapemodel ${INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
