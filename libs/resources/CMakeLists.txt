find_package(ICU REQUIRED COMPONENTS uc)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_subdirectory(tests)

# sql/manifest.tsv is the durable alias-to-source SOT. The generated source is
# a real input of kritaresources; the legacy resource file is intentionally not
# part of this path, so later cleanup can delete it without losing the mapping.
set(kritaresources_SQL_MANIFEST ${CMAKE_CURRENT_SOURCE_DIR}/sql/manifest.tsv)
set(kritaresources_SQL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/KisSqlScripts.h)
set(kritaresources_SQL_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/KisSqlScripts.cpp)
file(GLOB kritaresources_SQL_INPUTS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/sql/*.sql)
add_custom_command(
    OUTPUT ${kritaresources_SQL_HEADER} ${kritaresources_SQL_SOURCE}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_sql_scripts.py
            --manifest ${kritaresources_SQL_MANIFEST}
            --output-header ${kritaresources_SQL_HEADER}
            --output-source ${kritaresources_SQL_SOURCE}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/generate_sql_scripts.py
        ${kritaresources_SQL_MANIFEST}
        ${kritaresources_SQL_INPUTS}
    VERBATIM)

set(kritaresources_LIB_SRCS
    ${kritaresources_SQL_SOURCE}
    KisResourceCacheDb.cpp
    KisResourceLoader.cpp
    KisResourceLoaderRegistry.cpp
    KisResourceLocator.cpp
    KisResourceStorage.cpp
    KisResourceThumbnailCodec.cpp
    KisResourceModel.cpp
    KisTagFilterResourceProxyModel.cpp
    KisResourceModelProvider.cpp
    KisResourceTypeModel.cpp
    KisStorageModel.cpp
    KisStorageFilterProxyModel.cpp
    KisResourceIterator.cpp
    KisResourceSearchBoxFilter.cpp
    KisResourceQueryMapper.cpp
    KisResourceTypes.cpp

    KisStoragePlugin.cpp
    KisBundleStorage.cpp
    KisFolderStorage.cpp
    KisMemoryStorage.cpp

    KisTag.cpp
    KisTagModel.cpp

    KisTagResourceModel.cpp

    KoResource.cpp
    KoResourceBundle.cpp
    KoResourceBundleManifest.cpp
    KoMD5Generator.cpp
    MD5.cpp
    KoResourcePaths.cpp
    KoResourceSignature.cpp
    KoEmbeddedResource.cpp
    KoResourceLoadResult.cpp

    ResourceDebug.cpp

    KisRequiredResourcesOperators.cpp
    KisResourcesInterface.cpp
    KisLocalStrokeResources.cpp
    KisGlobalResourcesInterface.cpp
    KoResourceCacheInterface.cpp
    KoResourceCacheStorage.cpp
    KoResourceCachePrefixedStorageWrapper.cpp

    KoCanvasResourcesInterface.cpp
    KoLocalStrokeCanvasResources.cpp
    KisEmbeddedResourceStorageProxy.cpp

    KisResourceThumbnailCache.cpp
    KisResourceMetaDataModel.cpp
    KisSqlQueryLoader.cpp
    KisDatabaseTransactionLock.cpp
    KisTemporaryResourceStorageLock.cpp
    PkResourceStorageDesktop.cpp
)

kis_add_library(kritaresources SHARED ${kritaresources_LIB_SRCS})

generate_export_header(kritaresources BASE_NAME kritaresources)

target_include_directories(kritaresources
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/string
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/container
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/variant
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/pointer
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/geometry
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/global
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/log
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/concurrent
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/image
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/time
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/config
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/flags
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/color
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/signal
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/sql
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/xml
        ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/port
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../store
        ${CMAKE_CURRENT_SOURCE_DIR}/../global
        ${CMAKE_CURRENT_SOURCE_DIR}/../koplugin
)

target_link_libraries(kritaresources
    PUBLIC
        Boost::boost
    PRIVATE
        kritaversion
        kritaglobal
        kritaplugin
        kritastore
        pkstring
        pkcontainer
        pkvariant
        pkpointer
        pkport
        pksignal
        pkconcurrent
        pkconfig
        pklog
        pkimage
        pktime
        pksql
        pkxml
        pkgeometry
        pkglobal
        pkcolor
        pkflags
        PNG::PNG
        ICU::uc
)

set_target_properties(kritaresources PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritaresources ${INSTALL_TARGETS_DEFAULT_ARGS} )
