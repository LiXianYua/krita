kis_add_tests(
    TestPosition.cpp
    TestSelection.cpp
    TestPathTool.cpp
    TestShapeAt.cpp
    TestShapePainting.cpp
    TestKoShapeFactory.cpp
    TestShapeContainer.cpp
    TestShapeGroupCommand.cpp
    TestShapeReorderCommand.cpp
    TestResourceManager.cpp
    TestShapeBackgroundCommand.cpp
    TestShapeStrokeCommand.cpp
    TestInputDevice.cpp
    TestSnapStrategy.cpp
    TestPathShape.cpp
    TestControlPointMoveCommand.cpp
    TestPointTypeCommand.cpp
    TestPointRemoveCommand.cpp
    TestRemoveSubpathCommand.cpp
    TestPathSegment.cpp
    TestSegmentTypeCommand.cpp
    TestKoDrag.cpp
    TestKoMarkerCollection.cpp
    TestSvgSavingContext.cpp
    TestXsimdPainting.cpp
    TestFontLibraryResourceUtils.cpp
    TestSvgTextShape.cpp
    TestDependentShapes.cpp

    LINK_LIBRARIES kritaflake kritatestsdk
    )

kis_add_test(
    TestSvgParser.cpp
    TEST_NAME TestSvgParser
    LINK_LIBRARIES kritaflake kritatestsdk
    )

kis_add_test(
    TestSvgParser.cpp
    TEST_NAME TestSvgParserCloned
    LINK_LIBRARIES kritaflake kritatestsdk
    )
target_compile_definitions(TestSvgParserCloned PRIVATE USE_CLONED_SHAPES)

kis_add_test(
    TestSvgParser.cpp
    TEST_NAME TestSvgParserRoundTrip
    LINK_LIBRARIES kritaflake kritatestsdk
    )
target_compile_definitions(TestSvgParserRoundTrip PRIVATE USE_ROUND_TRIP)

kis_add_test(
    PkQPainterAdapterTest.cpp
    PkQPainterAdapter.cpp
    TEST_NAME PkQPainterAdapterTest
    LINK_LIBRARIES kritaflake kritatestsdk
    )
target_include_directories(PkQPainterAdapterTest PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/render
    )

kis_add_test(
    KoToolProxyPkPainterTest.cpp
    ../commands/KoParameterHandleMoveCommand.cpp
    TEST_NAME KoToolProxyPkPainterTest
    LINK_LIBRARIES kritaflake kritatestsdk
    )
target_include_directories(KoToolProxyPkPainterTest PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/render
    )

kis_add_test(
    KoCanvasDeferredCallTest.cpp
    TEST_NAME KoCanvasDeferredCallTest
    LINK_LIBRARIES kritaflake kritatestsdk
    )
target_include_directories(KoCanvasDeferredCallTest PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/concurrent
    )

kis_add_test(
    KoSvgTextHtmlNativeTest.cpp
    TEST_NAME KoSvgTextHtmlNativeTest
    LINK_LIBRARIES kritaflake kritatestsdk
    )

add_executable(KoSvgTextNewlineNativeTest
    KoSvgTextNewlineNativeTest.cpp
    ../text/KoSvgTextNewlineNormalizer.cpp
    )
target_include_directories(KoSvgTextNewlineNativeTest PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/string
    )
target_link_libraries(KoSvgTextNewlineNativeTest PRIVATE pkstring pkcontainer)
add_test(NAME libs-flake-KoSvgTextNewlineNativeTest
    COMMAND KoSvgTextNewlineNativeTest)

add_executable(KoFontGlyphModelNativeContractTest
    KoFontGlyphModelNativeContractTest.cpp
    )
target_include_directories(KoFontGlyphModelNativeContractTest BEFORE PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/flake/flake/noqt-compat
    ${CMAKE_SOURCE_DIR}/pk/string/compat
    ${CMAKE_SOURCE_DIR}/pk/container/compat
    ${CMAKE_SOURCE_DIR}/pk/variant/compat
    ${CMAKE_SOURCE_DIR}/pk/pointer/compat
    ${CMAKE_SOURCE_DIR}/pk/global/compat
    ${CMAKE_SOURCE_DIR}/pk/signal/compat
    ${CMAKE_SOURCE_DIR}/libs
    ${CMAKE_SOURCE_DIR}/libs/flake/text
    )
target_link_libraries(KoFontGlyphModelNativeContractTest PRIVATE
    pkstring pkcontainer pkvariant pksignal
    Freetype::Freetype HarfBuzz::HarfBuzz Fontconfig::Fontconfig)
add_test(NAME libs-flake-KoFontGlyphModelNativeContractTest
    COMMAND KoFontGlyphModelNativeContractTest)

############## broken tests ###############

kis_add_test(TestPointMergeCommand.cpp
    TEST_NAME TestPointMergeCommand
    LINK_LIBRARIES kritaflake kritatestsdk
    )

krita_add_broken_unit_test( TestSvgText.cpp
    TEST_NAME TestSvgText
    LINK_LIBRARIES kritaflake kritatestsdk
    )

krita_add_broken_unit_test( TestSvgText.cpp
    TEST_NAME TestSvgTextCloned
    LINK_LIBRARIES kritaflake kritatestsdk
    )
target_compile_definitions(TestSvgTextCloned PRIVATE USE_CLONED_SHAPES)

krita_add_broken_unit_test( TestSvgText.cpp
    TEST_NAME TestSvgTextRoundTrip
    LINK_LIBRARIES kritaflake kritatestsdk
    )
target_compile_definitions(TestSvgTextRoundTrip PRIVATE USE_ROUND_TRIP)

if (APPLE)
    set_property(TARGET TestKoMarkerCollection PROPERTY MACOSX_BUNDLE ON)
    set_property(TARGET TestSvgParser PROPERTY MACOSX_BUNDLE ON)
    set_property(TARGET TestSvgParserCloned PROPERTY MACOSX_BUNDLE ON)
    set_property(TARGET TestSvgParserRoundTrip PROPERTY MACOSX_BUNDLE ON)
endif()
