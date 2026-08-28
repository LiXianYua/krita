set(kritaanimation_LIB_SRCS
    KisAbstractFrameCacheSwapper.cpp
    KisFrameCacheStore.cpp
    KisFrameCacheSwapper.cpp
    KisFrameDataSerializer.cpp
    KisInMemoryFrameCacheSwapper.cpp
    kis_animation_frame_cache.cpp
    kis_animation_importer.cpp
)

kis_add_library(kritaanimation SHARED ${kritaanimation_LIB_SRCS})
generate_export_header(kritaanimation BASE_NAME kritaanimation)

target_include_directories(kritaanimation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/container>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/geometry>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/image>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/pointer>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/signal>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/string>
    PRIVATE
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/libs/store>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/concurrent>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/log>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/port>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/time>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/variant>
)

target_link_libraries(kritaanimation
    PUBLIC
        Boost::boost
        kritacanvas
        kritaglobal
        kritaimage
        kritaimpex
        pkcontainer
        pkgeometry
        pkimage
        pkpointer
        pksignal
        pkstring
    PRIVATE
        kritastore
        pkconcurrent
        pklog
        pktime
        pkvariant
)

set_target_properties(kritaanimation PROPERTIES
    AUTOMOC OFF
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritaanimation  ${INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
