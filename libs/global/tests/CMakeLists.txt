kis_add_tests(KisSignalAutoConnectionTest.cpp
    KisSignalCompressorTest.cpp
    KisForestTest.cpp
    KisMplTest.cpp
    KisRectsGridTest.cpp
    KisLazyStorageTest.cpp
    KisValueCacheTest.cpp
    KisHistoryListTest.cpp
    KisAdaptedLockTest.cpp
    KisGlobalTest.cpp
    KoPropertiesTest.cpp
    TestKoProgressUpdater.cpp
    LINK_LIBRARIES kritaglobal kritatestsdk
    )

if(BUILD_TESTING)
    add_executable(kritaglobal_file_io_test
        KisGlobalFileIoTest.cpp
        ../KisFileUtils.cpp
        ../KisBackup.cpp
        ../KisUsageLogger.cpp
        ../kis_assert.cpp
        ../kis_debug.cpp
    )
    target_include_directories(kritaglobal_file_io_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_BINARY_DIR}/..
        ${CMAKE_SOURCE_DIR}/libs/version
        ${CMAKE_SOURCE_DIR}/pk/string
        ${CMAKE_SOURCE_DIR}/pk/container
        ${CMAKE_SOURCE_DIR}/pk/pointer
        ${CMAKE_SOURCE_DIR}/pk/time
        ${CMAKE_SOURCE_DIR}/pk/port
        ${CMAKE_SOURCE_DIR}/pk/log
        ${CMAKE_SOURCE_DIR}/pk/global
        ${CMAKE_SOURCE_DIR}/pk/geometry
        ${CMAKE_SOURCE_DIR}/pk/color
        ${CMAKE_SOURCE_DIR}/pk/xml
    )
    target_link_libraries(kritaglobal_file_io_test PRIVATE
        kritaversion
        pkstring
        pkcontainer
        pkpointer
        pktime
        pkport
        pklog
        pkgeometry
        pkcolor
        pkxml
        Boost::boost
    )
    if(MINGW)
        target_link_options(kritaglobal_file_io_test PRIVATE -municode)
    endif()
    set_target_properties(kritaglobal_file_io_test PROPERTIES AUTOMOC OFF)
    add_test(NAME libs-global-KisGlobalFileIoTest COMMAND kritaglobal_file_io_test)
endif()
