set( extensions_plugin_SOURCES 
    extensions_plugin.cc
    kis_hsv_adjustment.cpp
    kis_dodgehighlights_adjustment.cpp
    kis_dodgemidtones_adjustment.cpp
    kis_dodgeshadows_adjustment.cpp
    kis_burnhighlights_adjustment.cpp
    kis_burnmidtones_adjustment.cpp
    kis_burnshadows_adjustment.cpp
    kis_color_balance_adjustment.cpp
    kis_desaturate_adjustment.cpp
)

kis_add_library(krita_colorspaces_extensions MODULE ${extensions_plugin_SOURCES})
set_target_properties(krita_colorspaces_extensions PROPERTIES AUTOMOC OFF)
target_link_libraries(krita_colorspaces_extensions PRIVATE
    kritapigment
    kritaglobal
    pkcontainer
    pkstring
    pkvariant
    ${LINK_OPENEXR_LIB}
)
install( TARGETS krita_colorspaces_extensions DESTINATION ${KRITA_PLUGIN_INSTALL_DIR} )
