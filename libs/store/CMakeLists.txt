set(kritastore_LIB_SRCS
    KoDirectoryStore.cpp
    KoStoreDevice.cpp
    KoStore.cpp
    PkFileStream.cpp
    PkMemoryStream.cpp
    KoXmlNS.cpp
    KoXmlWriter.cpp
    KoQuaZipStore.cpp
    StoreDebug.cpp
)

kis_add_library(kritastore SHARED ${kritastore_LIB_SRCS})
generate_export_header(kritastore BASE_NAME kritastore)

# pk 依赖（pk/string、pk/container、pk/variant、pk/port、pk/config）：include
# 路径照薄壳的 pk 依赖全景表。pk 目标是独立 CMake 工程不在本构建树里，
# target_link 不可用，只能接 include 路径；源文件与链接由后续把 pk 接进构建
# 系统的任务负责（同 libs/koplugin 的 D-19 预期断裂口径）。
# PUBLIC：KoStore.h 的公开接口直接暴露 PkString/PkStream/PkByteArray/PkStringList，
# 下游消费者也要能找到这些头。
target_include_directories(kritastore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/string
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/container
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/variant
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/port
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/config
)

set_target_properties(kritastore PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritastore ${INSTALL_TARGETS_DEFAULT_ARGS} )
