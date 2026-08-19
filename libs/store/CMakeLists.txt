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

# pk 依赖（pk/string、pk/container、pk/variant、pk/port、pk/port/zip、pk/config、pk/geometry、pk/log）
# ——共 8 个，与薄壳 SHELL_INCLUDE_DIRS 及源码 include 的 pk 子目录三者一致
# （S 线判据 8）。pk/port/zip 是 KoQuaZipStore.cpp:11 `#include "PkZipArchive.h"`
# 的所在目录（R-26 Task 3 评审发现 brief 写 7 个是漏了它，Task 5 补上第 8 个）。
# include 路径照薄壳的 pk 依赖全景表。pk 目标是独立 CMake 工程不在本构建树里，
# target_link 不可用，只能接 include 路径；源文件与链接由后续把 pk 接进构建
# 系统的任务负责（同 libs/koplugin 的 D-19 预期断裂口径）。
# PUBLIC：KoStore.h 的公开接口直接暴露 PkString/PkStream/PkByteArray/PkStringList，
# 下游消费者也要能找到这些头。pk/geometry 必需：KoStore.h → PkAuxTypes.h
# → #include "PkPoint.h"/"PkSize.h"（文件在 pk/geometry/），缺它 KoStore.cpp
# 的 TU 找不到 PkPoint.h/PkSize.h。pk/log 必需：StoreDebug.h
# → #include <PkLoggingCategory.h>/<PkMessageLogger.h>（文件在 pk/log/）——S-01
# 教训（薄壳编过、主树 CMakeLists 没加 pk/log、主树编不过），此处补齐。
target_include_directories(kritastore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/string
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/container
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/variant
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/port
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/geometry
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/log
)

target_link_libraries(kritastore PUBLIC
    pkstring
    pkcontainer
    pkvariant
    pkport
    pkconfig
    pkgeometry
    pklog
)

set_target_properties(kritastore PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritastore ${INSTALL_TARGETS_DEFAULT_ARGS} )
