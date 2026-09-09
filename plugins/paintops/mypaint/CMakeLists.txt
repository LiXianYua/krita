find_package(json-c CONFIG REQUIRED)
add_subdirectory(tests)
add_subdirectory(brushes)

set(kritamypaintop_SOURCES
    MyPaintBrushUtils.cpp
    MyPaintSensorPack.cpp
    MyPaintJson.cpp
    MyPaintCurveOptionData.cpp
    MyPaintCurveRangeModel.cpp
    MyPaintPaintOp.cpp
    MyPaintBasicOptionData.cpp
    MyPaintPaintOpSettings.cpp
    MyPaintSurface.cpp
    MyPaintPaintOpPreset.cpp
    MyPaintPaintOpFactory.cpp
    MyPaintStandardOptionData.cpp
)

kis_add_library(kritamypaintop_static STATIC ${kritamypaintop_SOURCES})

target_include_directories(kritamypaintop_static
    PRIVATE
        ${CMAKE_SOURCE_DIR}/plugins/paintops/libpaintop
        ${CMAKE_BINARY_DIR}/plugins/paintops/libpaintop
        ${CMAKE_SOURCE_DIR}/libs/brush
        ${CMAKE_BINARY_DIR}/libs/brush
        ${CMAKE_SOURCE_DIR}/libs/resources
        ${CMAKE_BINARY_DIR}/libs/resources
)

target_link_libraries(kritamypaintop_static
    PRIVATE
        LibMyPaint::mypaint
        kritalibpaintop_noqt_compile_interface
        PNG::PNG
        json-c::json-c)

kis_add_library(kritamypaintop STATIC MyPaintPaintOpPlugin.cpp)

target_link_libraries(kritamypaintop kritamypaintop_static kritalibpaintop_noqt_compile_interface)
set_target_properties(kritamypaintop_static kritamypaintop PROPERTIES AUTOMOC OFF)

install(TARGETS kritamypaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES  krita-mypaint.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)
