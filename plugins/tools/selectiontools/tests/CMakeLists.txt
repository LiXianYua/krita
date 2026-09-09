kis_add_test(selection_tools_test.cpp
    TEST_NAME SelectionToolsTest
    LINK_LIBRARIES kritaselectiontools kritatestsdk KF${KF_MAJOR}::ConfigCore
)

target_compile_definitions(SelectionToolsTest PRIVATE
    SELECTIONTOOLS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")

kis_add_test(selection_polygonal_dispatch_test.cpp
    TEST_NAME SelectionPolygonalDispatchTest
    LINK_LIBRARIES
        kritaselectiontools
        kritacanvas
        kritaflake
        kritashapemodel
        kritaimage
        kritaresources
        kritatestsdk
        Boost::boost
)

target_include_directories(SelectionPolygonalDispatchTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/libs/canvas
    ${CMAKE_SOURCE_DIR}/libs/canvas/tool
    ${CMAKE_SOURCE_DIR}/libs/flake
)
