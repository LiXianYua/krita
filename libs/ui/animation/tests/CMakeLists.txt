kis_add_tests(
    KisFrameSerializerTest.cpp
    KisFrameCacheStoreTest.cpp
    kis_animation_frame_cache_test.cpp
    LINK_LIBRARIES kritaanimation kritatestsdk
)

kis_add_test(
    kis_animation_importer_test.cpp
    TEST_NAME kis_animation_importer_test
    NAME_PREFIX libs-ui-
    LINK_LIBRARIES kritaanimation kritaimpex kritatestsdk
)

target_sources(KisFrameSerializerTest PRIVATE KisFrameSerializerTest.h)
target_sources(KisFrameCacheStoreTest PRIVATE KisFrameCacheStoreTest.h)
target_sources(kis_animation_frame_cache_test PRIVATE kis_animation_frame_cache_test.h)
target_sources(kis_animation_importer_test PRIVATE kis_animation_importer_test.h)
