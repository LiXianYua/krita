set(kritapsdutils_LIB_SRCS
    psd.cpp
    compression.cpp

    asl/kis_asl_reader.cpp
    asl/kis_asl_xml_parser.cpp
    asl/kis_asl_object_catcher.cpp
    asl/kis_asl_callback_object_catcher.cpp
    asl/kis_asl_xml_writer.cpp
    asl/kis_asl_patterns_writer.cpp
    asl/kis_asl_writer.cpp
    asl/kis_asl_byte_utils.cpp

    cos/kis_cos_parser.cpp
    cos/kis_cos_writer.cpp
    cos/kis_txt2_utls.cpp
    cos/PkCosMemoryStream.cpp
    cos/psd_text_data_converter.cpp
)

kis_add_library(kritapsdutils SHARED ${kritapsdutils_LIB_SRCS})
generate_export_header(kritapsdutils BASE_NAME kritapsdutils)

add_library(kritapsdutils_noqt_compile_interface INTERFACE)
target_include_directories(kritapsdutils_noqt_compile_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/cos
    ${CMAKE_SOURCE_DIR}/libs/global
    ${CMAKE_BINARY_DIR}/libs/global
    ${CMAKE_SOURCE_DIR}/libs/pigment
    ${CMAKE_BINARY_DIR}/libs/pigment
    ${CMAKE_SOURCE_DIR}/libs/pigment/resources
    ${CMAKE_SOURCE_DIR}/libs/pigment/compositeops
    ${CMAKE_SOURCE_DIR}/libs/flake
    ${CMAKE_BINARY_DIR}/libs/flake
    ${CMAKE_SOURCE_DIR}/libs/flake/flake
    ${CMAKE_BINARY_DIR}/libs/flake/flake
    ${CMAKE_SOURCE_DIR}/libs/flake/commands
    ${CMAKE_SOURCE_DIR}/libs/flake/tools
    ${CMAKE_SOURCE_DIR}/libs/flake/resources
    ${CMAKE_SOURCE_DIR}/libs/flake/svg
    ${CMAKE_SOURCE_DIR}/libs/flake/text
    ${CMAKE_SOURCE_DIR}/libs/resources
    ${CMAKE_BINARY_DIR}/libs/resources
)
target_link_libraries(kritapsdutils_noqt_compile_interface INTERFACE
        pkstring
        pkcontainer
        pkvariant
        pkport
        pkgeometry
        pklog
        pkxml
        pkpointer
        pkimage
        pkglobal
        pkconfig
        pkflags
        pksignal
        pkconcurrent
        pktime
        pksql
        pknamespace
        pkfont
        pktextstream
        Boost::boost
        Eigen3::Eigen
)

target_link_libraries(kritapsdutils PUBLIC
        kritapsdutils_noqt_compile_interface
        $<LINK_ONLY:kritapigment>
        $<LINK_ONLY:kritaglobal>
        $<LINK_ONLY:kritaflake>
)
target_link_libraries(kritapsdutils PRIVATE
        $<LINK_ONLY:ZLIB::ZLIB>
)

set_target_properties(kritapsdutils PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritapsdutils  ${INSTALL_TARGETS_DEFAULT_ARGS})

add_subdirectory(tests)
