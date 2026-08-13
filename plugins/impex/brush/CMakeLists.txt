add_subdirectory(tests)

set(kritabrushexport_PART_SRCS
    kis_brush_export.cpp
    KisAnimatedBrushAnnotation.cpp
    )

kis_add_library(kritabrushexport MODULE ${kritabrushexport_PART_SRCS})

target_link_libraries(kritabrushexport kritalibbrush kritaimpex)

install(TARGETS kritabrushexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritabrushimport_PART_SRCS
    kis_brush_import.cpp
    KisAnimatedBrushAnnotation.cpp
    )

ki18n_wrap_ui(kritabrushimport_PART_SRCS )

kis_add_library(kritabrushimport MODULE ${kritabrushimport_PART_SRCS})

target_link_libraries(kritabrushimport kritalibbrush kritaimpex kritaui)

install(TARGETS kritabrushimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_brush.desktop  DESTINATION ${KDE_INSTALL_APPDIR})
