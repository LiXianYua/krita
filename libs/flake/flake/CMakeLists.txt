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
    BEFORE PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/noqt-compat
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
        ${CMAKE_SOURCE_DIR}/libs
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../tool>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/canvas>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/libs/canvas>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/brush>
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/libs/brush>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/resources
)

target_link_libraries(kritashapemodel
    PUBLIC
        kritaimage_noqt_compile_interface
        pktextstream
        pkrender
        $<LINK_ONLY:kritaflake>
        $<LINK_ONLY:kritaimage>
        $<LINK_ONLY:kritalibbrush>
        $<LINK_ONLY:kritacanvas>

    PRIVATE
        kritaflake_imagecodec
        kritaresources
        $<LINK_ONLY:KF${KF_MAJOR}::I18n>
)

set_target_properties(kritashapemodel PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION}
    SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritashapemodel ${INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
