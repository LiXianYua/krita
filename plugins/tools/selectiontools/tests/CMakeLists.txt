kis_add_test(selection_tools_test.cpp
    TEST_NAME SelectionToolsTest
    LINK_LIBRARIES kritaselectiontools kritatestsdk KF${KF_MAJOR}::ConfigCore
)

target_compile_definitions(SelectionToolsTest PRIVATE
    SELECTIONTOOLS_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/..")
