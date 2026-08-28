add_subdirectory(tests)

set(kritaqmlexport_SOURCES
    qml_format.cc
    qml_converter.cc
    qml_export.cc
    )

kis_add_library(kritaqmlexport MODULE ${kritaqmlexport_SOURCES})

target_link_libraries(kritaqmlexport kritaimpex kritastore kritapigment)

install(TARGETS kritaqmlexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
