set(kritakrzexport_SOURCES
    krz_export.cpp
)

kis_add_library(kritakrzexport STATIC ${kritakrzexport_SOURCES})

target_link_libraries(kritakrzexport PUBLIC kritaimpex_noqt_compile_interface)
target_link_libraries(kritakrzexport INTERFACE $<LINK_ONLY:kritalibkra> $<LINK_ONLY:kritaimpex> $<LINK_ONLY:kritaimage>)
add_dependencies(kritakrzexport kritalibkra kritaimpex kritaimage)

install(TARGETS kritakrzexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_krz.desktop DESTINATION ${KDE_INSTALL_APPDIR})
