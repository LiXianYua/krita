set(kritaexifcommon_SOURCES
    KisExiv2IODevice.cpp
)

kis_add_library(kritaexifcommon SHARED ${kritaexifcommon_SOURCES})

generate_export_header(kritaexifcommon)

target_link_libraries(kritaexifcommon
    kritaglobal
    kritastore      # PkFileStream（S-01 交付）
    kritametadata   # KisMetaData::Value
    pkstring
    pkcontainer
    pkvariant
    pktime
    pkpointer
    pklog
    pkglobal
    LibExiv2::LibExiv2)

set_target_properties(kritaexifcommon PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritaexifcommon ${INSTALL_TARGETS_DEFAULT_ARGS})
