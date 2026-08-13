set(kritaanimation_LIB_SRCS
    KisAbstractFrameCacheSwapper.cpp
    KisFrameCacheStore.cpp
    KisFrameCacheSwapper.cpp
    KisFrameDataSerializer.cpp
    KisInMemoryFrameCacheSwapper.cpp
    kis_animation_frame_cache.cpp
)

kis_add_library(kritaanimation SHARED ${kritaanimation_LIB_SRCS})
generate_export_header(kritaanimation BASE_NAME kritaanimation)

target_include_directories(kritaanimation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(kritaanimation
    PUBLIC
        Boost::boost
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Gui
        kritacanvas
        kritaglobal
        kritaimage
    PRIVATE
        KF${KF_MAJOR}::ConfigCore
)

set_target_properties(kritaanimation PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritaanimation  ${INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
