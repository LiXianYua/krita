add_subdirectory( tests )

set(kritacommand_LIB_SRCS
	kundo2stack.cpp
	kundo2magicstring.cpp
	kundo2commandextradata.cpp
        kis_undo_store.cpp
        kis_undo_stores.cpp
        kis_command_utils.cpp
        KisCumulativeUndoData.cpp
)

kis_add_library(kritacommand SHARED ${kritacommand_LIB_SRCS})
generate_export_header(kritacommand BASE_NAME kritacommand)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(kritacommand_SIGNAL_HEADERS
    kundo2stack.h
    kis_undo_store.h
)
set(kritacommand_SIGNAL_SRCS)
foreach(signal_header IN LISTS kritacommand_SIGNAL_HEADERS)
    get_filename_component(signal_name "${signal_header}" NAME_WE)
    set(signal_source "${CMAKE_CURRENT_BINARY_DIR}/${signal_name}_pk_signals.cpp")
    add_custom_command(
        OUTPUT "${signal_source}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
                -o "${signal_source}"
        DEPENDS
                "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
        COMMENT "pk_signal_moc: generating ${signal_name} signal definitions"
        VERBATIM
    )
    list(APPEND kritacommand_SIGNAL_SRCS "${signal_source}")
endforeach()

target_sources(kritacommand PRIVATE ${kritacommand_SIGNAL_SRCS})
set_target_properties(kritacommand PROPERTIES AUTOMOC OFF)

target_link_libraries(kritacommand
    PUBLIC
        Boost::boost
        pkstring
        pkcontainer
        pktime
        pklog
        pksignal
        pkpointer
        pkconfig
        pkglobal
    PRIVATE
        kritaglobal
)

set_target_properties(kritacommand PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritacommand ${INSTALL_TARGETS_DEFAULT_ARGS})
