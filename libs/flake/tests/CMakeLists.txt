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


# R-65 T2+4 现场实测：TestShapeStrokeCommand **不可迁**（保留在真 Qt 测试栈）。
# 实测命令：`ninja -C build-r65 TestShapeStrokeCommand`
#   → FAILED: pk_wrap_TestShapeStrokeCommand.cpp.o
#     /…/QtCore.framework/Headers/qglobal.h:658: error: redefinition of 'qAbs'
#     (note: previous definition is here → pk/test/compat/QtGlobal:21)
# 根因：pk_add_test 只链 kritaflake，而 kritaflake **PUBLIC** 链
#   Qt5::Svg / KF5::WidgetsAddons / Qt5::Gui / Qt5::Xml (libs/flake/CMakeLists.txt:615-620)
# ⇒ 编译行里带回 -DQT_CORE_LIB/-DQT_WIDGETS_LIB… 与 Qt 框架 -isystem 头路径，
#   真 Qt 的 qglobal.h/qstring.h 与 pk compat 垫片同 TU 相撞。
# 这与「kis_shape_commands_test/kis_shape_layer_test/KisNodeDummiesGraphTest/
# KisNodeShapesGraphTest/KoGamutMaskTest/TestShapeBackgroundCommand」同一个墙：
# kritaflake 尚未剥 Qt（S 线 / S-08 后续）。在它对 pk 测试栈变得可用之前，
# libs/flake 下的全部 target 都迁不了。
