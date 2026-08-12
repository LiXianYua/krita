set(kritacurvepaintop_SOURCES
      curve_paintop_plugin.cpp
      kis_curve_paintop.cpp
      kis_curve_paintop_settings.cpp
      KisCurveOpOptionData.cpp

    )

kis_add_library(kritacurvepaintop MODULE ${kritacurvepaintop_SOURCES})

target_link_libraries(kritacurvepaintop kritalibpaintop)

install(TARGETS kritacurvepaintop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES  krita-curve.png DESTINATION ${KDE_INSTALL_DATADIR}/krita/images)
