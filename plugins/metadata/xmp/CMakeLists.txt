find_package(ICU REQUIRED COMPONENTS uc)

set(kritaxmp_SOURCES
    kis_xmp_io.cpp
    kis_xmp_plugin.cpp
)

kis_add_library(kritaxmp MODULE ${kritaxmp_SOURCES})

generate_export_header(kritaxmp)

target_link_libraries(kritaxmp
    PRIVATE
        kritametadata
        kritaexifcommon
        pkstring
        pkcontainer
        pkvariant
        pkport
        pktime
        pklog
        ICU::uc
        LibExiv2::LibExiv2
)

install(TARGETS kritaxmp DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
