find_package(json-c CONFIG REQUIRED)
add_subdirectory(tests)
add_subdirectory(brushes)

set(kritamypaintop_SOURCES
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
)

target_link_libraries(kritamypaintop_static
    PRIVATE
        LibMyPaint::mypaint
        kritalibbrush
        kritaresources
        kritalibpaintop
        PNG::PNG
        json-c::json-c)

kis_add_library(kritamypaintop MODULE MyPaintPaintOpPlugin.cpp)

target_link_libraries(kritamypaintop kritamypaintop_static kritalibpaintop)

install(TARGETS kritamypaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES  krita-mypaint.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)
