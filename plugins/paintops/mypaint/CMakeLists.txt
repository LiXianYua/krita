add_subdirectory(tests)
add_subdirectory(brushes)

set(kritamypaintop_SOURCES
    MyPaintSensorPack.cpp
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

target_link_libraries(kritamypaintop_static kritalibpaintop LibMyPaint::mypaint kritawidgetutils kritaui kritalibbrush kritaresources)

kis_add_library(kritamypaintop MODULE MyPaintPaintOpPlugin.cpp)

target_link_libraries(kritamypaintop kritamypaintop_static)

install(TARGETS kritamypaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES  krita-mypaint.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)
