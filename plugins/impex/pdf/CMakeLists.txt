find_package(Poppler REQUIRED COMPONENTS Cpp)

add_subdirectory(tests)

set(kritapdfimport_SOURCES kis_pdf_import.cpp pdf_import_policy.cpp)

kis_add_library(kritapdfimport MODULE ${kritapdfimport_SOURCES})

target_link_libraries(kritapdfimport kritaimpex Poppler::Cpp)

install(TARGETS kritapdfimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_pdf.desktop DESTINATION ${KDE_INSTALL_APPDIR})
