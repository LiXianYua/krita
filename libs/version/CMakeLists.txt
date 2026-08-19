# generate kritaversion.h
configure_file(kritaversion.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kritaversion.h)

# generate kritagitversion.h
configure_file(kritagitversion.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kritagitversion.h)

set(kritaversion_LIB_SRCS
    KritaVersionWrapper.cpp
)

kis_add_library(kritaversion SHARED ${kritaversion_LIB_SRCS})
generate_export_header(kritaversion BASE_NAME kritaversion)

# pk/string + pk/container：KritaVersionWrapper.h 的公开接口用 PkString（S-01
# 从 QString 迁移后缺，M1 验收发现，主会话补 2026-08-18）。PUBLIC——下游也要
# 能找到这个头。
target_include_directories(kritaversion PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/string
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/container
)

target_link_libraries(kritaversion PUBLIC
    pkstring
    pkcontainer
)

set_target_properties(kritaversion PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritaversion ${INSTALL_TARGETS_DEFAULT_ARGS} )

