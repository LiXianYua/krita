# KisDocumentReplaceTest.cpp:93 无条件 #include 了同 TU 的 binder（pk/test/CMakeLists.txt
# 的 ODR 硬规则），而 KisDocumentReplaceTest 在主树里仍是真 QObject + Q_OBJECT，靠
# Qt AUTOMOC + QTest 做槽发现。历史遗留的同 TU binder include 必须解析成一个 no-op。
# 形制照 sdk/tests/CMakeLists.txt:20-23。
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_kisdocumentreplacetest.inc"
    CONTENT "// Main-tree QObject branch: QTest discovers slots through moc.\n"
)

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

kis_add_test(
    KisPngCodecNoResourceDirsTest.cpp
    LINK_LIBRARIES kritaimpex kritaimage kritalcmsengine
)
target_include_directories(KisPngCodecNoResourceDirsTest PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/color/lcms2engine
)

# R-61 Task 3：(c) forceSRGB 降级语义的常驻载体。plain main()、零 Qt 头；
# 比 R-59 的 KisPngCodecNoResourceDirsTest 多链 ZLIB::ZLIB —— 读回的 iCCP chunk
# 载荷是 zlib 压缩的，比对前要解压成 ICC 字节。
kis_add_test(
    KisPngCodecForceSrgbNoResourceDirsTest.cpp
    LINK_LIBRARIES kritaimpex kritaimage kritalcmsengine ZLIB::ZLIB
)
target_include_directories(KisPngCodecForceSrgbNoResourceDirsTest PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/color/lcms2engine
)
