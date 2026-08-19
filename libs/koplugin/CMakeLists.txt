set(kritaplugin_LIB_SRCS
    KisMimeDatabase.cpp
)

kis_add_library(kritaplugin SHARED ${kritaplugin_LIB_SRCS})
generate_export_header(kritaplugin BASE_NAME kritaplugin)

# pkconfig（pk/config）依赖：include 路径复刻 pk/config/CMakeLists.txt 的 PUBLIC
# include 目录（pk/config 自身 + pk/string + pk/container + pk/geometry）。
# pkconfig 不在本构建树里（独立 CMake 工程），target_link 不可用，只能接 include
# 路径；源文件与链接由后续「把 pk 接进构建系统」的任务负责（D-19 预期断裂）。
target_include_directories(kritaplugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/string
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/container
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/geometry
)

set_target_properties(kritaplugin PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritaplugin ${INSTALL_TARGETS_DEFAULT_ARGS} )
