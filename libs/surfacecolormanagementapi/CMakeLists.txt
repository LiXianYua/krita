set(kritasurfacecolormanagementapi_LIB_SRCS
    KisColorimetryUtils.cpp
    surfacecolormanagement/KisSurfaceColorimetry.cpp
    surfacecolormanagement/KisSurfaceColorimetryIccUtils.cpp
)

kis_add_library(kritasurfacecolormanagementapi SHARED ${kritasurfacecolormanagementapi_LIB_SRCS})
generate_export_header(kritasurfacecolormanagementapi BASE_NAME kritasurfacecolormanagementapi)

target_link_libraries(kritasurfacecolormanagementapi
    PRIVATE
    kritaglobal
    kritapigment
    pkstring
    pkcontainer
    pklog
    pkgeometry
    pkglobal)

set_target_properties(kritasurfacecolormanagementapi PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritasurfacecolormanagementapi  ${INSTALL_TARGETS_DEFAULT_ARGS})

