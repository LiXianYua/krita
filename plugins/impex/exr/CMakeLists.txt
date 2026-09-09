add_subdirectory(tests)

set(libkritaconverter_LIB_SRCS
    exr_converter.cc
    exr_import_policy.cpp
    kis_exr_layers_sorter.cpp
    exr_extra_tags.cpp
    )

set(kritaexrimport_SOURCES
    exr_import.cc
    ${libkritaconverter_LIB_SRCS}
    )

kis_add_library(kritaexrimport STATIC ${kritaexrimport_SOURCES})

target_link_libraries(kritaexrimport PUBLIC kritaimpex_noqt_compile_interface OpenEXR::IlmImf pkconfig)
target_link_libraries(kritaexrimport INTERFACE $<LINK_ONLY:kritaimpex> $<LINK_ONLY:kritalibkra>)
add_dependencies(kritaexrimport kritaimpex kritalibkra)

install(TARGETS kritaexrimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritaexrexport_SOURCES
    exr_export.cc
    ${libkritaconverter_LIB_SRCS}
    )

kis_add_library(kritaexrexport STATIC ${kritaexrexport_SOURCES})

target_link_libraries(kritaexrexport PUBLIC kritaimpex_noqt_compile_interface OpenEXR::IlmImf pkconfig)
target_link_libraries(kritaexrexport INTERFACE $<LINK_ONLY:kritalibkra> $<LINK_ONLY:kritaimpex>)
add_dependencies(kritaexrexport kritalibkra kritaimpex)

install(TARGETS kritaexrexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install( FILES
    exrchannels.schema
    DESTINATION ${KDE_INSTALL_DATADIR}/krita/metadata/schemas)

install(FILES krita_exr.desktop DESTINATION ${KDE_INSTALL_APPDIR})
