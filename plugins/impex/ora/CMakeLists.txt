add_subdirectory(tests)

set(libkritaconverter_LIB_SRCS
    ora_converter.cpp
    kis_open_raster_load_context.cpp
    kis_open_raster_save_context.cpp
    kis_open_raster_stack_load_visitor.cpp
    kis_open_raster_stack_save_visitor.cpp
    ora_save_policy.cpp
    )

set(kritaoraimport_SOURCES
    ora_import.cc
    ${libkritaconverter_LIB_SRCS}
    )

kis_add_library(kritaoraimport STATIC ${kritaoraimport_SOURCES})

target_link_libraries(kritaoraimport PUBLIC kritaimpex_noqt_compile_interface)
target_link_libraries(kritaoraimport INTERFACE $<LINK_ONLY:kritaimpex>)
add_dependencies(kritaoraimport kritaimpex)

install(TARGETS kritaoraimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritaoraexport_SOURCES
    ora_export.cc
    ${libkritaconverter_LIB_SRCS}
    )

kis_add_library(kritaoraexport STATIC ${kritaoraexport_SOURCES})

target_link_libraries(kritaoraexport PUBLIC kritaimpex_noqt_compile_interface)
target_link_libraries(kritaoraexport INTERFACE $<LINK_ONLY:kritaimpex>)
add_dependencies(kritaoraexport kritaimpex)

install(TARGETS kritaoraexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_ora.desktop DESTINATION ${KDE_INSTALL_APPDIR})
