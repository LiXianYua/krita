project(kritaflake)

include_directories(
    ${CMAKE_SOURCE_DIR}/libs/flake/commands
    ${CMAKE_SOURCE_DIR}/libs/flake/tools
    ${CMAKE_SOURCE_DIR}/libs/flake/svg
    ${CMAKE_SOURCE_DIR}/libs/flake/text
    ${CMAKE_SOURCE_DIR}/libs/flake/xsimd
    ${CMAKE_SOURCE_DIR}/libs/flake/resources
    ${CMAKE_BINARY_DIR}/libs/flake
)

add_subdirectory(styles)
add_subdirectory(tests)
add_subdirectory(resources/tests)
add_subdirectory(flake)

set(DISABLE_CLIP_MASK_PAINTER_ON_MACOS FALSE)
if (APPLE)
    set(DISABLE_CLIP_MASK_PAINTER_ON_MACOS TRUE)
endif()

# generate krita_xsimd_macos_workaround.h
configure_file(krita_xsimd_macos_workaround.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/krita_xsimd_macos_workaround.h)

if(NOT DISABLE_CLIP_MASK_PAINTER_ON_MACOS)
    ko_compile_for_all_implementations(__per_arch_alpha_applicator_factory_objs xsimd/KoClipMaskApplicatorFactoryImpl.cpp)
else()
    set(__per_arch_alpha_applicator_factory_objs xsimd/KoClipMaskApplicatorFactoryImpl.cpp)
endif()

set(kritaflake_SRCS
    KoGradientHelper.cpp
    KoFlake.cpp
    KoCanvasBase.cpp
    KoResourceManager_p.cpp
    KoDerivedResourceConverter.cpp
    KoActiveCanvasResourceDependency.cpp
    KoAbstractCanvasResourceInterface.cpp
    KoResourceUpdateMediator.cpp
    KoCanvasResourceProvider.cpp
    KoDocumentResourceManager.cpp
    KoCanvasObserverBase.cpp
    KoCanvasSupervisor.cpp
    KoInsets.cpp
    KoPathShape.cpp
    KoPathPoint.cpp
    KoPathSegment.cpp
    KoSelection.cpp
    KoSelectedShapesProxy.cpp
    KoSelectedShapesProxySimple.cpp
    KoShape.cpp
    KoShapeAnchor.cpp
    KoShapeBulkActionLock.cpp
    KoShapeControllerBase.cpp
    KoShapeContainer.cpp
    KoShapeContainerModel.cpp
    KoShapeGroup.cpp
    KoShapeManager.cpp
    KoMarker.cpp
    KoMarkerCollection.cpp
    KoToolBase.cpp
    KoCanvasController.cpp
    KoZoomMode.cpp
    KoZoomState.cpp
    KoShapeRegistry.cpp
    KoDeferredShapeFactoryBase.cpp
    KoToolFactoryBase.cpp
    KoPathShapeFactory.cpp
    KoShapeFactoryBase.cpp
    KoShapeUserData.cpp
    KoParameterShape.cpp
    KoPointerEvent.cpp
    KoShapeController.cpp
    KoToolSelection.cpp
    KoShapeLayer.cpp
    KoInputDevice.cpp
    KoToolManager_p.cpp
    KoToolManager.cpp
    KoToolRegistry.cpp
    KoToolProxy.cpp
    KoShapeSavingContext.cpp
    KoShapeLoadingContext.cpp
    KoLoadingShapeUpdater.cpp
    KoPathShapeLoader.cpp
    KoShapeStrokeModel.cpp
    KoShapeStroke.cpp
    KoShapeBackground.cpp
    KoColorBackground.cpp
    KoGradientBackground.cpp
    KoMeshGradientBackground.cpp
    KoHatchBackground.cpp
    KoPatternBackground.cpp
    KoVectorPatternBackground.cpp
    KoShapeFillWrapper.cpp
    KoShapeFillResourceConnector.cpp
    KoDrag.cpp
    KoSvgPaste.cpp
    KoSnapGuide.cpp
    KoSnapProxy.cpp
    KoSnapStrategy.cpp
    KisSnapPointStrategy.cpp
    KoSnapData.cpp
    KoSharedLoadingData.cpp
    KoSharedSavingData.cpp
    KoViewConverter.cpp
    KoViewTransformStillPoint.cpp
    KoClipMaskApplicatorBase.cpp
    ${__per_arch_alpha_applicator_factory_objs}
    KoClipMaskPainter.cpp
    KoClipPath.cpp
    KoClipMask.cpp
    KoCurveFit.cpp

    commands/KoShapeGroupCommand.cpp
    commands/KoShapeAlignCommand.cpp
    commands/KoShapeBackgroundCommand.cpp
    commands/KoShapeCreateCommand.cpp
    commands/KoShapeDeleteCommand.cpp
    commands/KoShapeDistributeCommand.cpp
    commands/KoShapeLockCommand.cpp
    commands/KoShapeMoveCommand.cpp
    commands/KoShapeResizeCommand.cpp
    commands/KoShapeShearCommand.cpp
    commands/KoShapeSizeCommand.cpp
    commands/KoShapeStrokeCommand.cpp
    commands/KoShapeUngroupCommand.cpp
    commands/KoShapeReorderCommand.cpp
    commands/KoShapeKeepAspectRatioCommand.cpp
    commands/KoPathBaseCommand.cpp
    commands/KoPathPointMoveCommand.cpp
    commands/KoPathControlPointMoveCommand.cpp
    commands/KoPathPointTypeCommand.cpp
    commands/KoPathPointRemoveCommand.cpp
    commands/KoPathPointInsertCommand.cpp
    commands/KoPathSegmentBreakCommand.cpp
    commands/KoPathBreakAtPointCommand.cpp
    commands/KoPathSegmentTypeCommand.cpp
    commands/KoPathCombineCommand.cpp
    commands/KoSubpathRemoveCommand.cpp
    commands/KoSubpathJoinCommand.cpp
    commands/KoParameterHandleMoveCommand.cpp
    commands/KoParameterToPathCommand.cpp
    commands/KoShapeTransformCommand.cpp
    commands/KoPathFillRuleCommand.cpp
    commands/KoPathReverseCommand.cpp
    commands/KoShapeRenameCommand.cpp
    commands/KoPathPointMergeCommand.cpp
    commands/KoShapeTransparencyCommand.cpp
    commands/KoShapeClipCommand.cpp
    commands/KoShapeUnclipCommand.cpp
    commands/KoPathShapeMarkerCommand.cpp
    commands/KoMultiPathPointMergeCommand.cpp
    commands/KoMultiPathPointJoinCommand.cpp
    commands/KoKeepShapesSelectedCommand.cpp
    commands/KoPathMergeUtils.cpp
    commands/KoAddRemoveShapeCommands.cpp
    commands/KoShapePaintOrderCommand.cpp
    commands/KoShapeMergeTextPropertiesCommand.cpp
    commands/KoSvgConvertTextTypeCommand.cpp
    commands/KoSvgTextAddRemoveShapeCommands.cpp
    commands/KoSvgTextReorderShapeInsideCommand.cpp
    commands/KoSvgTextFlipShapeContourTypeCommand.cpp
    commands/KoSvgTextPathInfoChangeCommand.cpp

    html/HtmlSavingContext.cpp
    html/HtmlWriter.cpp

    tools/KoPathToolFactory.cpp
    tools/KoPathTool.cpp
    tools/KoPathToolSelection.cpp
    tools/KoPathToolHandle.cpp
    tools/KoPathPointRubberSelectStrategy.cpp
    tools/KoPathPointMoveStrategy.cpp
    tools/KoPathControlPointMoveStrategy.cpp
    tools/KoParameterChangeStrategy.cpp
    tools/KoZoomTool.cpp
    tools/KoZoomToolFactory.cpp
    tools/KoZoomStrategy.cpp
    tools/KoInteractionTool.cpp
    tools/KoInteractionStrategy.cpp
    tools/KoInteractionStrategyFactory.cpp
    tools/KoShapeRubberSelectStrategy.cpp
    tools/KoPathSegmentChangeStrategy.cpp
    tools/KoCreatePathTool.cpp
    tools/KoPencilTool.cpp

    svg/KoShapePainter.cpp
    svg/SvgUtil.cpp
    svg/SvgGraphicContext.cpp
    svg/SvgSavingContext.cpp
    svg/SvgWriter.cpp
    svg/SvgStyleWriter.cpp
    svg/SvgShape.cpp
    svg/SvgParser.cpp
    svg/SvgStyleParser.cpp
    svg/SvgGradientHelper.cpp
    svg/SvgCssHelper.cpp
    svg/SvgClipPathHelper.cpp
    svg/SvgLoadingContext.cpp
    svg/parsers/SvgTransformParser.cpp
    svg/SvgMeshGradient.cpp
    svg/SvgMeshPatch.cpp
    svg/SvgMeshArray.cpp

    text/KisTofuGlyph.cpp
    text/KoSvgText.cpp
    text/KoSvgTextProperties.cpp
    text/KoSvgTextContentElement.cpp
    text/KoSvgTextShape.cpp
    text/KoSvgTextLoader.cpp
    text/KoSvgTextShape_p_glyphs.cpp
    text/KoSvgTextShape_p_layout.cpp
    text/KoSvgTextShape_p_output.cpp
    text/KoSvgTextShapeLayoutFunc_inShape.cpp
    text/KoSvgTextShapeLayoutFunc_lines.cpp
    text/KoSvgTextShapeMarkupConverter.cpp
    text/KoFontRegistry.cpp
    text/KoCssTextUtils.cpp
    text/KoPolygonUtils.cpp
    text/KoSvgTextPropertyData.cpp
    text/KoSvgTextPropertiesInterface.h
    text/KoFFWWSConverter.cpp
    text/KoFontChangeTracker.cpp
    text/KoWritingSystemUtils.cpp
    text/KoFontGlyphModel.cpp
    text/KoOpenTypeFeatureInfoFactory.cpp
    text/data/KoUnicodeBlockData.cpp
    text/KoSvgTextShapeOutlineHelper.cpp

    resources/KoSvgSymbolCollectionResource.cpp
    resources/KoGamutMask.cpp
    resources/KoFontFamily.cpp
    resources/KoFontStorage.cpp
    resources/KoCssStylePreset.cpp

    FlakeDebug.cpp
    tests/MockShapes.cpp

)

if (KSeExpr_FOUND)
    set(kritaflake_SRCS ${kritaflake_SRCS}
        resources/KisSeExprScript.cpp
    )
endif()

set(kritaflake_OBJECT_DEPS
    kritapigment
    kritacommand
    KF${KF_MAJOR}::WidgetsAddons
    Qt${QT_MAJOR_VERSION}::Svg
    KF${KF_MAJOR}::CoreAddons
    KF${KF_MAJOR}::ConfigCore
    KF${KF_MAJOR}::I18n
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Xml
    Boost::boost
    Freetype::Freetype
    HarfBuzz::HarfBuzz
    Raqm::Raqm
    Fontconfig::Fontconfig
    libunibreak::libunibreak
)

if (QT_MAJOR_VERSION STREQUAL "6")
    list(APPEND kritaflake_OBJECT_DEPS Qt${QT_MAJOR_VERSION}::OpenGLWidgets)
endif()

function(kritaflake_add_object_library target)
    kis_add_library(${target} OBJECT ${ARGN})
    target_link_libraries(${target} PRIVATE ${kritaflake_OBJECT_DEPS})
    target_compile_definitions(${target} PRIVATE kritaflake_EXPORTS)
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

set(kritaflake_commands_SRCS ${kritaflake_SRCS})
list(FILTER kritaflake_commands_SRCS INCLUDE REGEX "^commands/")

set(kritaflake_html_SRCS ${kritaflake_SRCS})
list(FILTER kritaflake_html_SRCS INCLUDE REGEX "^html/")

set(kritaflake_tools_SRCS ${kritaflake_SRCS})
list(FILTER kritaflake_tools_SRCS INCLUDE REGEX "^tools/")

set(kritaflake_svg_SRCS ${kritaflake_SRCS})
list(FILTER kritaflake_svg_SRCS INCLUDE REGEX "^svg/")

set(kritaflake_text_SRCS ${kritaflake_SRCS})
list(FILTER kritaflake_text_SRCS INCLUDE REGEX "^text/")

set(kritaflake_resources_SRCS ${kritaflake_SRCS})
list(FILTER kritaflake_resources_SRCS INCLUDE REGEX "^resources/")

set(kritaflake_core_SRCS ${kritaflake_SRCS})
list(FILTER kritaflake_core_SRCS EXCLUDE REGEX "^(commands|html|tools|svg|text|resources)/")
list(REMOVE_ITEM kritaflake_core_SRCS ${__per_arch_alpha_applicator_factory_objs})

# ===== S-08 Task 6：上层绘图过渡层（保留 Qt，不进薄壳）=====
# 分类口径：文件自身（.cpp 去 include 行 或 配对 .h）出现无 Pk 替代的
# 绘图/UI/排版硬类型（QPainter/QPen/QBrush/QWidget/QFont/QTextLayout/QTextDocument/
# 事件族/QAction/QCursor/QClipboard 等，见 S-08 impact-map §compat 缺口 1+2 类）
# → 归入上层绘图，保留过渡 Qt；不建 PkPainter/PkPen/PkBrush（S-02-b 裁决）。
# 含 S-02-b 交接的 4 个 libs/global 闭包（本任务搬入 libs/flake）与 Qt 版 KisAlgebra2D。
set(kritaflake_painting_SRCS
    kis_painting_tweaks.cpp
    KisHandlePainterHelper.cpp
    KisHandleStyle.cpp
    KisQPainterStateSaver.cpp
    KisAlgebra2D.cpp
    KoCanvasBase.cpp
    KoCanvasController.cpp
    KoCanvasResourceProvider.cpp
    KoClipMask.cpp
    KoClipMaskPainter.cpp
    KoClipPath.cpp
    KoColorBackground.cpp
    KoDocumentResourceManager.cpp
    KoDrag.cpp
    KoGradientBackground.cpp
    KoHatchBackground.cpp
    KoInputDevice.cpp
    KoMarker.cpp
    KoMeshGradientBackground.cpp
    KoParameterShape.cpp
    KoPathPoint.cpp
    KoPathShape.cpp
    KoPatternBackground.cpp
    KoPointerEvent.cpp
    KoSelection.cpp
    KoShape.cpp
    KoShapeAnchor.cpp
    KoShapeBackground.cpp
    KoShapeContainer.cpp
    KoShapeFillWrapper.cpp
    KoShapeGroup.cpp
    KoShapeLayer.cpp
    KoShapeManager.cpp
    KoShapeStroke.cpp
    KoShapeStrokeModel.cpp
    KoSnapGuide.cpp
    KoSvgPaste.cpp
    KoToolBase.cpp
    KoToolFactoryBase.cpp
    KoToolManager.cpp
    KoToolManager_p.cpp
    KoToolProxy.cpp
    KoVectorPatternBackground.cpp
    resources/KoGamutMask.cpp
    resources/KoSvgSymbolCollectionResource.cpp
    svg/KoShapePainter.cpp
    svg/SvgParser.cpp
    svg/SvgStyleWriter.cpp
    svg/SvgWriter.cpp
    tests/MockShapes.cpp
    text/KoFFWWSConverter.cpp
    text/KoFontRegistry.cpp
    text/KoSvgText.cpp
    text/KoSvgTextProperties.cpp
    text/KoSvgTextShape.cpp
    text/KoSvgTextShapeMarkupConverter.cpp
    text/KoSvgTextShapeOutlineHelper.cpp
    text/KoSvgTextShape_p_glyphs.cpp
    text/KoSvgTextShape_p_layout.cpp
    text/KoSvgTextShape_p_output.cpp
    text/KoWritingSystemUtils.cpp
    tools/KoCreatePathTool.cpp
    tools/KoInteractionStrategy.cpp
    tools/KoInteractionStrategyFactory.cpp
    tools/KoInteractionTool.cpp
    tools/KoPathTool.cpp
    tools/KoPathToolFactory.cpp
    tools/KoPathToolHandle.cpp
    tools/KoPathToolSelection.cpp
    tools/KoPencilTool.cpp
    tools/KoShapeRubberSelectStrategy.cpp
    tools/KoZoomTool.cpp
)

# 绘图文件从各组摘走，避免重复编译（REMOVE_ITEM 精确匹配，跨组无效）
list(REMOVE_ITEM kritaflake_commands_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_html_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_tools_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_svg_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_text_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_resources_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_core_SRCS ${kritaflake_painting_SRCS})

kritaflake_add_object_library(kritaflake_painting_objects ${kritaflake_painting_SRCS})
# KisAlgebra2D（DecomposedMatrix）用 Eigen 做矩阵分解——Eigen 原为 libs/global 的
# PRIVATE 依赖（kis_algebra_2d.cpp 随闭包搬入 flake），这里随搬移补上。
target_link_libraries(kritaflake_painting_objects PRIVATE Eigen3::Eigen)

kritaflake_add_object_library(kritaflake_commands_objects ${kritaflake_commands_SRCS})
kritaflake_add_object_library(kritaflake_html_objects ${kritaflake_html_SRCS})
kritaflake_add_object_library(kritaflake_tools_objects ${kritaflake_tools_SRCS})
kritaflake_add_object_library(kritaflake_svg_objects ${kritaflake_svg_SRCS})
kritaflake_add_object_library(kritaflake_text_objects ${kritaflake_text_SRCS})
kritaflake_add_object_library(kritaflake_resources_objects ${kritaflake_resources_SRCS})
kritaflake_add_object_library(kritaflake_core_objects ${kritaflake_core_SRCS})

kis_add_library(kritaflake SHARED
    $<TARGET_OBJECTS:kritaflake_commands_objects>
    $<TARGET_OBJECTS:kritaflake_html_objects>
    $<TARGET_OBJECTS:kritaflake_tools_objects>
    $<TARGET_OBJECTS:kritaflake_svg_objects>
    $<TARGET_OBJECTS:kritaflake_text_objects>
    $<TARGET_OBJECTS:kritaflake_resources_objects>
    $<TARGET_OBJECTS:kritaflake_core_objects>
    $<TARGET_OBJECTS:kritaflake_painting_objects>
    ${__per_arch_alpha_applicator_factory_objs}
)
generate_export_header(kritaflake BASE_NAME kritaflake)

target_include_directories(kritaflake
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/commands>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tools>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/svg>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/text>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
)

set_target_properties(libraqm PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(kritaflake
    PUBLIC
        kritapigment
        kritacommand
        KF${KF_MAJOR}::WidgetsAddons
        Qt${QT_MAJOR_VERSION}::Svg
        KF${KF_MAJOR}::CoreAddons
        KF${KF_MAJOR}::ConfigCore
        KF${KF_MAJOR}::I18n
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Xml
        Boost::boost
        Freetype::Freetype
        HarfBuzz::HarfBuzz
    PRIVATE
        Raqm::Raqm
        Fontconfig::Fontconfig
        libunibreak::libunibreak
)

if (QT_MAJOR_VERSION STREQUAL "6")
target_link_libraries(kritaflake
    PRIVATE
        Qt${QT_MAJOR_VERSION}::OpenGLWidgets
)
endif()

set_target_properties(kritaflake PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritaflake  ${INSTALL_TARGETS_DEFAULT_ARGS})
