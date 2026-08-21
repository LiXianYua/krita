add_subdirectory( tests )

include(CheckFunctionExists)
check_function_exists(backtrace HAVE_BACKTRACE)
configure_file(config-debug.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-debug.h)

option(HAVE_MEMORY_LEAK_TRACKER "Enable memory leak tracker (always disabled in release build)" OFF)
option(HAVE_BACKTRACE_SUPPORT "Enable recording of backtrace in memory leak tracker" OFF)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config-memory-leak-tracker.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-memory-leak-tracker.h) ### WRONG PLACE???

set(kritaglobal_LIB_SRCS
    kis_assert.cpp
    kis_debug.cpp
    kis_algebra_2d.cpp
    kis_memory_leak_tracker.cpp
    kis_shared.cpp
    kis_dom_utils.cpp
    KisHandlePainterHelper.cpp
    KisHandleStyle.cpp
    kis_signal_compressor.cpp
    kis_signal_compressor_with_param.cpp
    kis_thread_safe_signal_compressor.cpp
    kis_acyclic_signal_connector.cpp
    kis_latency_tracker.cpp
    KisQPainterStateSaver.cpp
    KisRollingMeanAccumulatorWrapper.cpp
    KisRollingSumAccumulatorWrapper.cpp
    kis_config_notifier.cpp
    KisDeleteLaterWrapper.cpp
    KisUsageLogger.cpp
    KisGlobalFileSystem.cpp
    KisFileUtils.cpp
    KisSignalMapper.cpp
    KisRegion.cpp
    KoID.cpp
    KoUnit.cpp
    KisBezierUtils.cpp
    KisBezierPatch.cpp
    KisBezierMesh.cpp
    KisRectsGrid.cpp
    KisSynchronizedConnection.cpp
    KisFilteredRollingMean.cpp
    KisBackup.cpp
    KisSampleRectIterator.cpp
    KisCursorOverrideLock.cpp
    kis_random_source.cpp
    KisMessageBoxWrapper.cpp
    KisTransformComponents.cpp
    KoProgressProxy.cpp
    KoFakeProgressProxy.cpp
    KoProgressUpdater.cpp
    KoUpdater.cpp
    KoUpdaterPrivate_p.cpp
    KoProperties.cpp
)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(kritaglobal_SIGNAL_HEADERS
    KoProgressUpdater.h
    KoUpdater.h
    KoUpdaterPrivate_p.h
    kis_acyclic_signal_connector.h
    kis_config_notifier.h
    kis_signal_compressor.h
    kis_signal_compressor_with_param.h
    kis_thread_safe_signal_compressor.h
    KisSignalMapper.h
)

set(kritaglobal_SIGNAL_SRCS)
foreach(signal_header IN LISTS kritaglobal_SIGNAL_HEADERS)
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
    list(APPEND kritaglobal_SIGNAL_SRCS "${signal_source}")
endforeach()

if(WIN32)
    set(kritaglobal_LIB_SRCS
        ${kritaglobal_LIB_SRCS}
        KisWindowsPackageUtils.cpp
    )
endif(WIN32)

if (ANDROID)
    list(APPEND kritaglobal_LIB_SRCS
        KisAndroidCrashHandler.cpp
        KisAndroidExitInfo.cpp
        KisAndroidLogHandler.cpp
        KisAndroidUtils.cpp
    )
    include_directories(${unwindstack_INCLUDE_DIR})
endif()

kis_add_library(kritaglobal SHARED ${kritaglobal_LIB_SRCS}
   )

target_sources(kritaglobal PRIVATE ${kritaglobal_SIGNAL_SRCS})
set_target_properties(kritaglobal PROPERTIES AUTOMOC OFF)

target_sources(kritaglobal
  PRIVATE
    KisMessageBoxWrapper.h
    KisMessageBoxWrapper.cpp
)
generate_export_header(kritaglobal BASE_NAME kritaglobal)

target_link_libraries(kritaglobal
    PUBLIC
        kritaversion
        pkstring
        pkcontainer
        pkgeometry
        pkconcurrent
        pklog
        pksignal
        pkxml
        pktime
        pkvariant
        pkconfig
        pkpointer
        pkflags
        pkport
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Widgets
        Qt${QT_MAJOR_VERSION}::Xml
        KF${KF_MAJOR}::I18n
        KF${KF_MAJOR}::ConfigCore
        Boost::boost
        zug
        lager
    PRIVATE
        Eigen3::Eigen
)

if (QT_MAJOR_VERSION STREQUAL "6")
  target_link_libraries(kritaglobal PUBLIC Qt6::Core5Compat Qt6::OpenGL)
endif()

if (GSL_FOUND)
  target_link_libraries(kritaglobal PRIVATE GSL::gsl)
endif ()

if (ANDROID)
    if (QT_MAJOR_VERSION STREQUAL "6")
        target_link_libraries(kritaglobal PRIVATE ${unwindstack_LIBRARIES} log)
    else()
        target_link_libraries(kritaglobal PRIVATE ${unwindstack_LIBRARIES} Qt5::AndroidExtras log)
    endif()
endif()

if(APPLE)
    target_link_libraries(kritaglobal PRIVATE
        kritamacosutils
    )
endif ()

set_target_properties(kritaglobal PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritaglobal  ${INSTALL_TARGETS_DEFAULT_ARGS})
