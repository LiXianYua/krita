add_subdirectory(tests)

if (TIFF_HAS_PSD_TAGS)
    set(libkritatiffpsd_SRCS
        kis_tiff_psd_layer_record.cpp
        kis_tiff_psd_resource_record.cpp
    )

    kis_add_library(kritatiffpsd SHARED ${libkritatiffpsd_SRCS})
    generate_export_header(kritatiffpsd)

    set_target_properties(kritatiffpsd PROPERTIES
        VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
    )

    target_link_libraries(kritatiffpsd PUBLIC
        kritaimpex_noqt_compile_interface
        kritapsd_noqt_compile_interface
        ${TIFF_LIBRARIES}
    )
    target_link_libraries(kritatiffpsd PRIVATE $<LINK_ONLY:kritapsd>)
    add_dependencies(kritatiffpsd kritapsd)

    install(TARGETS kritatiffpsd DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS})

    set(KRITATIFFPSD_LIBRARY kritatiffpsd)
endif()

set(kritatiffimport_SOURCES
    kis_tiff_import.cc
    kis_buffer_stream.cc
)

kis_add_library(kritatiffimport STATIC ${kritatiffimport_SOURCES})

target_link_libraries(kritatiffimport
    PUBLIC
        kritaimpex_noqt_compile_interface
        LibExiv2::LibExiv2
        kritametadata
        kritaexifcommon
        ${JPEG_TURBO_LIBRARIES}
        ${KRITATIFFPSD_LIBRARY}
        ${TIFF_LIBRARIES}
    INTERFACE
        $<LINK_ONLY:kritaimpex>
    )
add_dependencies(kritatiffimport kritaimpex)

install(TARGETS kritatiffimport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

set(kritatiffexport_SOURCES
    kis_tiff_base_writer.cpp
    kis_tiff_converter.cc
    kis_tiff_writer_visitor.cpp
    kis_tiff_export.cc
)

if(TIFF_CAN_WRITE_PSD_TAGS)
set(kritatiffexport_SOURCES
    ${kritatiffexport_SOURCES}
    kis_tiff_psd_writer_visitor.cpp
)
endif()

kis_add_library(kritatiffexport STATIC ${kritatiffexport_SOURCES})

target_link_libraries(kritatiffexport
    PUBLIC
        kritaimpex_noqt_compile_interface
        LibExiv2::LibExiv2
        kritaexifcommon
        ${KRITATIFFPSD_LIBRARY}
        ${TIFF_LIBRARIES}
    INTERFACE
        $<LINK_ONLY:kritaimpex>
    )
add_dependencies(kritatiffexport kritaimpex)

install(TARGETS kritatiffexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install(FILES krita_tiff.desktop DESTINATION ${KDE_INSTALL_APPDIR})
