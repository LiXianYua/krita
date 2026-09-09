# export

add_subdirectory(tests)

set(kritaspriterexport_SOURCES
    kis_spriter_export.cpp
    spriter_format.cpp
)

kis_add_library(kritaspriterexport STATIC ${kritaspriterexport_SOURCES})

target_link_libraries(kritaspriterexport PUBLIC kritaimpex_noqt_compile_interface pkxml)
target_link_libraries(kritaspriterexport INTERFACE $<LINK_ONLY:kritaimpex>)
add_dependencies(kritaspriterexport kritaimpex)

install(TARGETS kritaspriterexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_spriter.desktop DESTINATION ${KDE_INSTALL_APPDIR})
