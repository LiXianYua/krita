set(kritapsd_SRCS
    psd_additional_layer_info_block.cpp
    psd_header.cpp
    psd_layer_record.cpp
    psd_layer_section.cpp
    psd_pixel_utils.cpp
    psd_resource_block.cpp
    psd_resource_section.cpp
)

kis_add_library(kritapsd SHARED ${kritapsd_SRCS})
generate_export_header(kritapsd)

add_library(kritapsd_noqt_compile_interface INTERFACE)
target_include_directories(kritapsd_noqt_compile_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/libs/flake
    ${CMAKE_BINARY_DIR}/libs/flake
    ${CMAKE_SOURCE_DIR}/libs/flake/flake
    ${CMAKE_BINARY_DIR}/libs/flake/flake
)
target_link_libraries(kritapsd_noqt_compile_interface INTERFACE
    kritaimage_noqt_compile_interface
    kritapsdutils_noqt_compile_interface
)

target_link_libraries(kritapsd PUBLIC
        kritapsd_noqt_compile_interface
        $<LINK_ONLY:kritaimage>
        $<LINK_ONLY:kritapsdutils>
        $<LINK_ONLY:kritashapemodel>
)

set_target_properties(kritapsd PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION}
    SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritapsd ${INSTALL_TARGETS_DEFAULT_ARGS})
