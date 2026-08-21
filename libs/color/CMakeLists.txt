set(kritacolor_LIB_SRCS
    kis_color_manager.h
    dummy/kis_color_manager.cpp
)

kis_add_library(kritacolor SHARED ${kritacolor_LIB_SRCS})
generate_export_header(kritacolor BASE_NAME kritacolor)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(kritacolor_SIGNAL_HEADERS kis_color_manager.h)
set(kritacolor_SIGNAL_SRCS)
foreach(signal_header IN LISTS kritacolor_SIGNAL_HEADERS)
    get_filename_component(signal_name "${signal_header}" NAME_WE)
    set(signal_source "${CMAKE_CURRENT_BINARY_DIR}/${signal_name}_pk_signals.cpp")
    add_custom_command(
        OUTPUT "${signal_source}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
                -o "${signal_source}"
        DEPENDS "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
        COMMENT "pk_signal_moc: generating ${signal_name} signal definitions"
        VERBATIM)
    list(APPEND kritacolor_SIGNAL_SRCS "${signal_source}")
endforeach()
target_sources(kritacolor PRIVATE ${kritacolor_SIGNAL_SRCS})
set_target_properties(kritacolor PROPERTIES AUTOMOC OFF)

target_link_libraries(kritacolor
    PRIVATE
        kritaglobal
        pkstring
        pkcontainer
        pkvariant
        pksignal
)

set_target_properties(kritacolor PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritacolor ${INSTALL_TARGETS_DEFAULT_ARGS})
