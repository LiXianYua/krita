set(kritamultiarch_SRCS
    KisSupportedArchitectures.cpp
    KoMultiArchBuildSupport.cpp
)

kis_add_library(kritamultiarch SHARED ${kritamultiarch_SRCS})

generate_export_header(kritamultiarch)

target_link_libraries(kritamultiarch PUBLIC xsimd)

set_target_properties(kritamultiarch PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

target_include_directories(kritamultiarch
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>
    PUBLIC
        # pk/string、pk/container、pk/config、pk/geometry、pk/log：KoMultiArchBuildSupport.h
        # 的公开接口 include PkMessageLogger.h（pk/log），KisSupportedArchitectures.cpp 用
        # PkConfigGroup/PkSharedConfig（pk/config）、PkString/PkStringList（pk/string）。
        # S-01 从 Qt 迁移后缺，M1 验收发现，主会话补 2026-08-18。PUBLIC——下游也要能找到。
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/string
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/container
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/config
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/geometry
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/log
)

install(TARGETS kritamultiarch ${INSTALL_TARGETS_DEFAULT_ARGS})
