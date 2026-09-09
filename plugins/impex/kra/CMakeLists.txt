set(kritakraimport_SOURCES
    kra_import.cpp
)

kis_add_library(kritakraimport STATIC ${kritakraimport_SOURCES})

target_include_directories(kritakraimport PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/impex/libkra
    ${CMAKE_BINARY_DIR}/plugins/impex/libkra
)

target_link_libraries(kritakraimport PUBLIC kritaimpex_noqt_compile_interface)
target_link_libraries(kritakraimport INTERFACE $<LINK_ONLY:kritaimpex> $<LINK_ONLY:kritalibkra>)
add_dependencies(kritakraimport kritaimpex kritalibkra)

install(TARGETS kritakraimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritakraexport_SOURCES
    kra_export.cpp
)

kis_add_library(kritakraexport STATIC ${kritakraexport_SOURCES})

target_include_directories(kritakraexport PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/impex/libkra
    ${CMAKE_BINARY_DIR}/plugins/impex/libkra
)

target_link_libraries(kritakraexport PUBLIC kritaimpex_noqt_compile_interface)
target_link_libraries(kritakraexport INTERFACE $<LINK_ONLY:kritalibkra> $<LINK_ONLY:kritaimpex>)
add_dependencies(kritakraexport kritalibkra kritaimpex)

install(TARGETS kritakraexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_kra.desktop DESTINATION ${KDE_INSTALL_APPDIR})

if(SHOULD_BUILD_FILEMANAGER_THUMBNAIL)
    install( FILES  krita_kra_thumbnail.desktop  DESTINATION ${SERVICES_INSTALL_DIR})
endif()
