kis_add_test(
    KisDocumentReplaceTest.cpp
    LINK_LIBRARIES kritaimpex kritatestsdk
)

kis_add_test(
    KisPkImageFileDecoderAdapterTest.cpp
    ../../../plugins/paintops/spray/KisSprayShapeOptionData.cpp
    TEST_NAME KisPkImageFileDecoderAdapterTest
    LINK_LIBRARIES kritaimpex pkimageio
)
target_include_directories(KisPkImageFileDecoderAdapterTest PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/paintops/spray
    ${CMAKE_SOURCE_DIR}/pk/image
)
