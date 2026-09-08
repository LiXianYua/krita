set(kritapalettize_SOURCES palettize.cpp)
kis_add_library(kritapalettize STATIC ${kritapalettize_SOURCES})
set_target_properties(kritapalettize PROPERTIES AUTOMOC OFF)
target_link_libraries(kritapalettize PUBLIC
    kritafilterdithering kritaimage_noqt_compile_interface)
target_link_libraries(kritapalettize INTERFACE
    $<LINK_ONLY:kritaimage>)
add_dependencies(kritapalettize kritaimage)
install(TARGETS kritapalettize  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
install( FILES
    palettize.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
