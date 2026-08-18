set(kritapaintkernel_LIB_SRCS
    KisPaintKernelStageMarker.cpp
)

kis_add_library(paintkernel SHARED ${kritapaintkernel_LIB_SRCS})
generate_export_header(paintkernel BASE_NAME paintkernel)

target_link_libraries(paintkernel
    PUBLIC
    kritaversion
    kritaglobal
    kritaplugin
    kritastore
    kritaflake
    kritapigment
    kritacommand
    kritalibbrush
    kritapsdutils
    kritacolor
    kritaimage
    kritacanvas
    kritainput
    kritaanimation
    kritaimpex
    kritaresources
    kritametadata
    kritapsd
    kritamultiarch
    kritasurfacecolormanagementapi
)

target_link_options(paintkernel PRIVATE "-Wl,--no-as-needed")

set_target_properties(paintkernel PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS paintkernel ${INSTALL_TARGETS_DEFAULT_ARGS})
