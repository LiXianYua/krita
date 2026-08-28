add_subdirectory(tests)

set(krita_raw_import_SOURCES
    kis_raw_import.cpp
)

kis_add_library(krita_raw_import MODULE ${krita_raw_import_SOURCES})
target_link_libraries(krita_raw_import kritaimpex)

install(TARGETS krita_raw_import  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_raw.desktop DESTINATION ${KDE_INSTALL_APPDIR})
