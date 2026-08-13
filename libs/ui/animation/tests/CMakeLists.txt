kis_add_tests(
    KisFrameSerializerTest.cpp
    KisFrameCacheStoreTest.cpp
    kis_animation_frame_cache_test.cpp
    LINK_LIBRARIES kritaanimation kritatestsdk
)

target_sources(KisFrameSerializerTest PRIVATE KisFrameSerializerTest.h)
target_sources(KisFrameCacheStoreTest PRIVATE KisFrameCacheStoreTest.h)
target_sources(kis_animation_frame_cache_test PRIVATE kis_animation_frame_cache_test.h)
