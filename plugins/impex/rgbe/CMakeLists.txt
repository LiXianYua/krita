add_subdirectory(tests)

add_library(kritargbeimportutils STATIC RGBEImportUtils.cpp rgbe_codec.cpp)
target_link_libraries(kritargbeimportutils PUBLIC kritaimage)

set(kritargbeimport_SOURCES
    RGBEImport.cpp
)

add_library(kritargbeimport MODULE ${kritargbeimport_SOURCES})
target_link_libraries(kritargbeimport kritaimpex kritargbeimportutils)

install(TARGETS kritargbeimport DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritargbeexport_SOURCES
    RGBEExport.cpp
)

kis_add_library(kritargbeexport MODULE ${kritargbeexport_SOURCES})

target_link_libraries(kritargbeexport kritaimpex)

install(TARGETS kritargbeexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_rgbe.desktop DESTINATION ${KDE_INSTALL_APPDIR})
