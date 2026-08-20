include_directories(${QUAZIP_INCLUDE_DIRS})
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
)

kis_add_library(kritaresources SHARED ${kritaresources_LIB_SRCS})

generate_export_header(kritaresources BASE_NAME kritaresources)

target_include_directories(kritaresources PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(kritaresources
    PUBLIC
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Widgets
        Qt${QT_MAJOR_VERSION}::Sql
        Boost::boost
    PRIVATE
        kritaversion
        kritaglobal
        kritaplugin
        kritastore
        KF${KF_MAJOR}::ConfigCore
        KF${KF_MAJOR}::CoreAddons
        KF${KF_MAJOR}::I18n
        ${QUAZIP_LIBRARIES}
        ${PNG_LIBRARIES}
        ICU::uc
)

set_target_properties(kritaresources PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritaresources ${INSTALL_TARGETS_DEFAULT_ARGS} )
