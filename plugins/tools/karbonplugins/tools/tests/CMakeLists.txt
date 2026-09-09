kis_add_test(KarbonToolsCoreTest.cpp
    TEST_NAME KarbonToolsCoreTest
    LINK_LIBRARIES
        krita_karbontools
        kritaflake
        kritacanvas
        Qt5::Gui
)
set_target_properties(KarbonToolsCoreTest PROPERTIES AUTOMOC OFF)
add_test(
    NAME plugins-tools-karbonplugins-tools-KarbonToolsResourceLateInstallTest
    COMMAND $<TARGET_FILE:KarbonToolsCoreTest> --late-install-after-skip)
add_test(
    NAME plugins-tools-karbonplugins-tools-KarbonToolsResourceReentryTest
    COMMAND $<TARGET_FILE:KarbonToolsCoreTest> --resource-reentry)
set_tests_properties(
    plugins-tools-karbonplugins-tools-KarbonToolsResourceReentryTest
    PROPERTIES TIMEOUT 5)
target_include_directories(KarbonToolsCoreTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/global
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/xml
)

add_test(
    NAME plugins-tools-karbonplugins-tools-KarbonToolsResourceIncrementalTest
    COMMAND ${CMAKE_COMMAND}
        -DKARBON_RESOURCE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -DKARBON_RESOURCE_FIXTURE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/resource_incremental_fixture
        -DKARBON_RESOURCE_FIXTURE_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/resource_incremental_fixture
        -DKARBON_RESOURCE_TEST_GENERATOR=${CMAKE_GENERATOR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/KarbonToolsResourceIncrementalTest.cmake)
set_tests_properties(
    plugins-tools-karbonplugins-tools-KarbonToolsResourceIncrementalTest
    PROPERTIES TIMEOUT 180)
