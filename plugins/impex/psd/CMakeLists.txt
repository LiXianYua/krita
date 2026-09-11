add_subdirectory(tests)

set(LIB_PSD_SRCS 
    psd_colormode_block.cpp
    psd_image_data.cpp
)

kis_add_library( kritapsd_static STATIC ${LIB_PSD_SRCS})

target_link_libraries(kritapsd_static PUBLIC
    kritaimpex_noqt_compile_interface
    kritapsd_noqt_compile_interface
)
target_link_libraries(kritapsd_static INTERFACE $<LINK_ONLY:kritaimage> $<LINK_ONLY:kritapsd>)
add_dependencies(kritapsd_static kritaimage kritapsd)

#
# import
#

set(kritapsdimport_SOURCES
    psd_import.cc
    psd_loader.cpp
)

kis_add_library(kritapsdimport STATIC ${kritapsdimport_SOURCES})

target_include_directories(kritapsdimport PRIVATE ${CMAKE_SOURCE_DIR}/libs/flake/text ${CMAKE_SOURCE_DIR}/pk/font ${CMAKE_SOURCE_DIR}/libs/flake/svg ${CMAKE_SOURCE_DIR}/libs/psd ${CMAKE_BINARY_DIR}/libs/psd)
target_link_libraries(kritapsdimport PUBLIC kritaimpex_noqt_compile_interface)
target_link_libraries(kritapsdimport INTERFACE $<LINK_ONLY:kritapsd_static> $<LINK_ONLY:kritaglobal> $<LINK_ONLY:kritaimpex>)
add_dependencies(kritapsdimport kritapsd_static kritaglobal kritaimpex)

install(TARGETS kritapsdimport DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

#
# export
#

set(kritapsdexport_SOURCES
    psd_export.cc
    psd_saver.cpp
)

kis_add_library(kritapsdexport STATIC ${kritapsdexport_SOURCES})

target_link_libraries(kritapsdexport PUBLIC
    kritaimpex_noqt_compile_interface
    kritapsd_noqt_compile_interface
)
target_link_libraries(kritapsdexport INTERFACE $<LINK_ONLY:kritapsd_static> $<LINK_ONLY:kritapsd> $<LINK_ONLY:kritaimpex> $<LINK_ONLY:kritastore>)
add_dependencies(kritapsdexport kritapsd_static kritapsd kritaimpex kritastore)

install(TARGETS kritapsdexport DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_psd.desktop DESTINATION ${KDE_INSTALL_APPDIR})
