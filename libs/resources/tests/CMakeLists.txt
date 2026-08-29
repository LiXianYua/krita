kis_add_tests(
    TestResourceCacheDb.cpp
    TestResourceLoaderRegistry.cpp
    TestResourceLocator.cpp
    TestResourceStorage.cpp
    TestFolderStorage.cpp
    TestMemoryStorage.cpp
    TestTag.cpp
    TestBundleStorage.cpp
    TestResourceModel.cpp
    TestTagFilterResourceProxyModel.cpp
    TestTagModel.cpp
    TestResourceTypeModel.cpp
    TestStorageModel.cpp
    TestResourceSearchBoxFilter.cpp
    TestStorageFilterProxyModel.cpp
    TestTagResourceModel.cpp
    LINK_LIBRARIES kritaglobal kritapigment kritaplugin kritaresources kritaversion kritatestsdk pksql pkconfig
    )

add_executable(KisResourceThumbnailCodecTest
    KisResourceThumbnailCodecTest.cpp)
target_link_libraries(KisResourceThumbnailCodecTest
    kritaresources pkport pkvariant pkimage)
target_compile_definitions(KisResourceThumbnailCodecTest PRIVATE
    PAINTOP_PRESET_DATA_DIR="${CMAKE_SOURCE_DIR}/libs/image/tests/data")
add_test(NAME libs-resources-KisResourceThumbnailCodecTest
    COMMAND KisResourceThumbnailCodecTest)
