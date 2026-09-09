kis_add_test(KarbonToolsCoreTest.cpp
    TEST_NAME KarbonToolsCoreTest
    LINK_LIBRARIES
        krita_karbontools
        kritaflake
        kritacanvas
        Qt5::Gui
)
set_target_properties(KarbonToolsCoreTest PROPERTIES AUTOMOC OFF)
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
