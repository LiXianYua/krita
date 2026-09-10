project(kritaflake)

include_directories(
    ${CMAKE_SOURCE_DIR}/libs/flake/commands
    ${CMAKE_SOURCE_DIR}/libs/flake/tools
    ${CMAKE_SOURCE_DIR}/libs/flake/svg
    ${CMAKE_SOURCE_DIR}/libs/flake/text
    ${CMAKE_SOURCE_DIR}/libs/flake/xsimd
    ${CMAKE_SOURCE_DIR}/libs/flake/resources
    ${CMAKE_BINARY_DIR}/libs/flake
    # S-08 Task 8（判据⑧/⑨）：flake 源码 include 面已含 pk 头（PkString/PkVariant/
    # PkDateTime/PkXmlElement 等），并经 PkXmlCompat.h（libs/pigment）激活
    # `#include <compat/QXxx>` 垫片——pk/<dir> 在 include 路径上即可解析到
    # pk/<dir>/compat/ 同名垫片。主树保留过渡 Qt（绘图层），故**只加 pk/<dir>，
    # 不加 pk/*/compat**——后者会把真实 Qt 头（QString/QtGlobal 等）遮蔽掉。
    # 与薄壳 SHELL_INCLUDE_DIRS 的 pk 段对齐（18 目录全列）；实测 pk 模块根无
    # Qt 同名头（Q*/q* 均只在各 compat/ 下），加全 18 目录无遮蔽风险。
    # 剥离期整树不编（决策 D-19），此路径面供编译层判据⑧核对。
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/port
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/global
    ${CMAKE_SOURCE_DIR}/pk/log
    ${CMAKE_SOURCE_DIR}/pk/concurrent
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/time
    ${CMAKE_SOURCE_DIR}/pk/config
    ${CMAKE_SOURCE_DIR}/pk/flags
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/sql
    ${CMAKE_SOURCE_DIR}/pk/xml
    ${CMAKE_SOURCE_DIR}/pk/namespace
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/font
    ${CMAKE_SOURCE_DIR}/pk/input
)

if(NOT TARGET pkrender)
    add_library(pkrender STATIC
        ${CMAKE_SOURCE_DIR}/pk/render/PkPainter.cpp
        ${CMAKE_SOURCE_DIR}/pk/render/PkStrokeOutline.cpp
        ${CMAKE_SOURCE_DIR}/pk/render/PkSvgRasterizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PkImageRasterBackend.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PkGrayRaster.c
        ${CMAKE_CURRENT_SOURCE_DIR}/PkAliasedRasterizer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/PkCosmeticStroker.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/svg/parsers/SvgTransformParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/svg/SvgCssHelper.cpp
    )
    target_include_directories(pkrender PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/render>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/image>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/font>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/string>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/namespace>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/svg/parsers>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/svg>
    )
    target_link_libraries(pkrender PUBLIC
        pkgeometry
        pkimage
        pkcolor
        pkfont
        pkxml
        pkimageio
        libunibreak::libunibreak
    )
    set_target_properties(pkrender PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# Native leaf shared by core SVG images and the image-shape plugin; no flake
# dependency here, so the plugin's existing flake edge cannot create a cycle.
add_library(kritaflake_imagecodec STATIC shapes/ImageShapePngData.cpp shapes/PkReferenceImageLoader.cpp)
set_target_properties(kritaflake_imagecodec PROPERTIES AUTOMOC OFF POSITION_INDEPENDENT_CODE ON)
target_link_libraries(kritaflake_imagecodec PUBLIC pkimage pkstring pkcontainer PNG::PNG TIFF::TIFF)
target_link_libraries(kritaflake_imagecodec PRIVATE pkimageio LibExiv2::LibExiv2 LCMS2::LCMS)
if(JPEG_FOUND)
    target_link_libraries(kritaflake_imagecodec PRIVATE JPEG::JPEG)
    target_compile_definitions(kritaflake_imagecodec PRIVATE HAVE_JPEG=1)
endif()

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
    svg/SvgClipPathHelper.cpp
    svg/SvgLoadingContext.cpp
    svg/SvgMeshGradient.cpp
    svg/SvgMeshPatch.cpp
    svg/SvgMeshArray.cpp

    text/KisTofuGlyph.cpp
    text/KoSvgText.cpp
    text/KoSvgTextProperties.cpp
    text/KoSvgTextContentElement.cpp
    text/KoSvgTextNewlineNormalizer.cpp
    text/KoSvgTextShape.cpp
    text/KoSvgTextLoader.cpp
    text/KoSvgTextShape_p_glyphs.cpp
    text/KoSvgTextShape_p_layout.cpp
    text/KoSvgTextShape_p_output.cpp
    text/KoSvgTextShapeLayoutFunc_inShape.cpp
    text/KoSvgTextShapeLayoutFunc_lines.cpp
    text/KoSvgTextShapeMarkupConverter.cpp
    text/KoFontRegistry.cpp
    text/KoFontGlyphModel.cpp
    text/KoFontProviderFontconfig.cpp
    text/KoLcLocale.cpp
    text/KoCssTextUtils.cpp
    text/KoPolygonUtils.cpp
    text/KoSvgTextPropertyData.cpp
    text/KoSvgTextPropertiesInterface.h
    text/KoFFWWSConverter.cpp
    text/KoFontChangeTracker.cpp
    text/KoWritingSystemUtils.cpp
    text/KoOpenTypeFeatureInfoFactory.cpp
    text/data/KoUnicodeBlockData.cpp
    text/KoSvgTextShapeOutlineHelper.cpp

    resources/KoSvgSymbolCollectionResource.cpp
    resources/KoGamutMask.cpp
    resources/KoFontFamily.cpp
    resources/KoFontFamilyMetadata.cpp
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

set(kritaflake_NATIVE_OBJECT_DEPS
    pktextstream
    kritapigment
    kritacommand
    Boost::boost
    Freetype::Freetype
    HarfBuzz::HarfBuzz
    Raqm::Raqm
    Fontconfig::Fontconfig
    libunibreak::libunibreak
    # S-08 Task 8（判据⑨）：pk 库显式接入 flake 编译/链接层。与 libs/pigment 的
    # PUBLIC 传递集一致（flake 已经 kritapigment 传递拿到，这里显式化）；
    # pkglobal 纯 header-only（inline constexpr）无需链；pksql/pktest flake 不用。
    pkstring pkcontainer pkcolor pkgeometry pkvariant pkxml pkimage pkport pkpointer
    pkconcurrent pklog pksignal pktime pkconfig pkflags pknamespace
    pkinput
)

set(kritaflake_QT_OBJECT_DEPS
    KF${KF_MAJOR}::WidgetsAddons
    Qt${QT_MAJOR_VERSION}::Svg
    KF${KF_MAJOR}::CoreAddons
    KF${KF_MAJOR}::ConfigCore
    KF${KF_MAJOR}::I18n
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Xml
)

if (QT_MAJOR_VERSION STREQUAL "6")
    list(APPEND kritaflake_QT_OBJECT_DEPS Qt${QT_MAJOR_VERSION}::OpenGLWidgets)
endif()

set(kritaflake_OBJECT_DEPS ${kritaflake_NATIVE_OBJECT_DEPS} ${kritaflake_QT_OBJECT_DEPS})

function(kritaflake_add_object_library target)
    kis_add_library(${target} OBJECT ${ARGN})
    target_link_libraries(${target} PRIVATE ${kritaflake_OBJECT_DEPS})
    target_compile_definitions(${target} PRIVATE kritaflake_EXPORTS)
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()

function(kritaflake_add_native_object_library target)
    kis_add_library(${target} OBJECT ${ARGN})
    target_include_directories(${target} BEFORE PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/flake/flake/noqt-compat
        ${CMAKE_SOURCE_DIR}/pk/string/compat
        ${CMAKE_SOURCE_DIR}/pk/container/compat
        ${CMAKE_SOURCE_DIR}/pk/variant/compat
        ${CMAKE_SOURCE_DIR}/pk/port/compat
        ${CMAKE_SOURCE_DIR}/pk/pointer/compat
        ${CMAKE_SOURCE_DIR}/pk/global/compat
        ${CMAKE_SOURCE_DIR}/pk/geometry/compat
        ${CMAKE_SOURCE_DIR}/pk/log/compat
        ${CMAKE_SOURCE_DIR}/pk/concurrent/compat
        ${CMAKE_SOURCE_DIR}/pk/image/compat
        ${CMAKE_SOURCE_DIR}/pk/time/compat
        ${CMAKE_SOURCE_DIR}/pk/config/compat
        ${CMAKE_SOURCE_DIR}/pk/flags/compat
        ${CMAKE_SOURCE_DIR}/pk/color/compat
        ${CMAKE_SOURCE_DIR}/pk/signal/compat
        ${CMAKE_SOURCE_DIR}/pk/sql/compat
        ${CMAKE_SOURCE_DIR}/pk/xml/compat
        ${CMAKE_SOURCE_DIR}/libs
    )
    target_link_libraries(${target} PRIVATE ${kritaflake_NATIVE_OBJECT_DEPS})
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

# ===== S-08 Task 6：上层绘图过渡层 =====
# 分类口径：文件自身（.cpp 去 include 行 或 配对 .h）出现无 Pk 替代的
# 绘图/UI/排版硬类型（QPainter/QPen/QBrush/QWidget/QFont/QTextLayout/QTextDocument/
# 事件族/QAction/QCursor/QClipboard 等，见 S-08 impact-map §compat 缺口 1+2 类）。
# 含 S-02-b 交接的 4 个 libs/global 闭包（本任务搬入 libs/flake）与 Qt 版 KisAlgebra2D。
#
# 归属以 S线-spec「已裁决的岔路」为准（2026-09-05、2026-09-09 两次裁决）：
#  - 本任务内真剥 Qt；`QPainter→PkPainter` 属 S-09-g（实测这层 QPainter 已修干净）。
#    旧文「不建 PkPainter/PkPen/PkBrush（S-02-b 裁决）」被 2026-09-05 后裁推翻，不再适用。
#  - 三分法：Qt UI 面直接删除（上层是 Flutter），Qt 非 UI 面实现 Pk 替代品，
#    暂无 Pk 等价物的走 flake/noqt-compat 垫片过渡。
#    宿主 GUI 面的 **UI 部分**（工具选项面板、工具坞）已在 S-09-g 步 8b 删除，因此不需要
#    为本对象库补 Qt include 路径。**但其余部分仍在本对象库里引用 Qt 头**——按三分法分派：
#    `QApplication::clipboard()` 归 PR-CLIPBOARD（Step 9）、平台查询与事件族归
#    PR-EVENT/PR-OBJECT（Step 5/6）、`QAction`/`QCursor` 走 flake/noqt-compat 垫片。
#    测量与机制见 plan §7.6.9 / §7.6.10，当前前沿见 root map §6/§7。
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
    shapes/ImageShape.cpp
    shapes/ImageShapeFactory.cpp
    shapes/RectangleShape.cpp
    shapes/RectangleShapeFactory.cpp
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
)

# 绘图文件从各组摘走，避免重复编译（REMOVE_ITEM 精确匹配，跨组无效）
list(REMOVE_ITEM kritaflake_commands_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_html_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_tools_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_svg_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_text_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_resources_SRCS ${kritaflake_painting_SRCS})
list(REMOVE_ITEM kritaflake_core_SRCS ${kritaflake_painting_SRCS})

# ===== S-09-g 定向移桶（2026-09-10 裁定，线级 spec「宿主 GUI 面按三分法处置」）=====
# `KoToolSelection` / `KoToolProxy` 携带**宿主 QObject 身份**（对象树 / QPointer /
# 事件循环）——2026-09-05 裁定「不剥，交接 M5」，本轮的落地形态就是这一条。
# 类头按桶分叉：Qt 桶是真 `QObject`，native 桶经 `pk/signal/compat/QObject` 把 `QObject`
# 宏成 `PkObject`（`KoToolProxy.h` 的 `#if defined(QT_CORE_LIB)` 双基写法即为此）。
# 于是**声明与定义必须同桶**：`KoToolProxy` 两套布局、全树只能有一份定义，定义留在哪一桶
# 就要按哪一桶的构造/析构签名产出符号，否则另一桶的 TU 链接期去要一个不存在的拼法，
# 报成一条看不出根因的未定义引用。
# 口径：**定向**移桶，只动这两个文件名，不重划对象库，也不动上面那段全局互斥
# `REMOVE_ITEM`（裁定原文点名不许动它）。下列各行必须写在那段之后、各 `*_objects`
# target 创建之前——写前面会被 `REMOVE_ITEM` 撤销，写后面该 target 已经定型。
#
# 【本轮实测边界，别再往下扩】`KoToolManager.cpp` / `KoCanvasController.cpp` /
# `KoToolFactoryBase.cpp` 曾被一并移进 Qt 桶试图让 oracle 跑起来，**已回退**。理由不是
# 「超出列举」，是**它们与 native 桶构成一个必须同桶的簇**：
# `KoCanvasController::actionCollection()` 的返回类型按桶分叉（Qt 桶真 `QObject*`、
# native 桶 `PkObject*`），而**返回类型不进 mangled name**（符号恒为
# `_ZN18KoCanvasController16actionCollectionEv`），所以它**永远链得过**；
# 一旦定义落在 Qt 桶，native 的 `KoToolBase.cpp:326` 就会把真 `QObject*` 当 `PkObject*`
# 用（在真 QObject 内存上操作 `PkObject` 的 `std::mutex`）——实测 `SIGSEGV` /
# `std::system_error: Invalid argument` / glibc assert。而 `KoToolBase.cpp`(native) 与
# `KoToolManager.cpp` 都消费这个面，于是 `{KoToolBase, KoToolManager, KoCanvasController,
# KoToolFactoryBase}` 必须同桶。**移动其中任何一个 = 重划对象库，裁定不许。**
list(REMOVE_ITEM kritaflake_painting_SRCS KoToolProxy.cpp)
list(REMOVE_ITEM kritaflake_core_SRCS KoToolSelection.cpp)
list(APPEND kritaflake_tools_SRCS KoToolProxy.cpp KoToolSelection.cpp)

kritaflake_add_native_object_library(kritaflake_painting_objects ${kritaflake_painting_SRCS})
set_target_properties(kritaflake_painting_objects PROPERTIES AUTOMOC OFF)
target_include_directories(kritaflake_painting_objects PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/font
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/namespace)
# KisAlgebra2D（DecomposedMatrix）用 Eigen 做矩阵分解——Eigen 原为 libs/global 的
# PRIVATE 依赖（kis_algebra_2d.cpp 随闭包搬入 flake），这里随搬移补上。
target_link_libraries(kritaflake_painting_objects PRIVATE Eigen3::Eigen kritaflake_imagecodec)

kritaflake_add_native_object_library(kritaflake_commands_objects ${kritaflake_commands_SRCS})
kritaflake_add_native_object_library(kritaflake_html_objects ${kritaflake_html_SRCS})
kritaflake_add_object_library(kritaflake_tools_objects ${kritaflake_tools_SRCS})
kritaflake_add_native_object_library(kritaflake_svg_objects ${kritaflake_svg_SRCS})
target_link_libraries(kritaflake_svg_objects PRIVATE kritaflake_imagecodec)
kritaflake_add_native_object_library(kritaflake_text_objects ${kritaflake_text_SRCS})
set_target_properties(kritaflake_text_objects PROPERTIES AUTOMOC OFF)
kritaflake_add_native_object_library(kritaflake_resources_objects ${kritaflake_resources_SRCS})
target_link_libraries(kritaflake_resources_objects PRIVATE kritaflake_imagecodec)
kritaflake_add_native_object_library(kritaflake_core_objects ${kritaflake_core_SRCS})

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
    # S-08 Task 8（判据⑧）：pk include 面 PUBLIC 传播给消费方（kritashapemodel /
    # 插件 / 测试）。只加 pk/<dir> 不加 pk/*/compat（见顶部 include_directories 注释，
    # 主树保留过渡 Qt 需要真实 Qt 头不被遮蔽）。
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/string>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/container>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/variant>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/port>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/pointer>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/geometry>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/global>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/log>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/concurrent>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/image>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/time>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/config>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/flags>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/color>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/signal>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/sql>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/xml>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/namespace>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/render>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/font>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/input>
)

set_target_properties(libraqm PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(kritaflake
    PRIVATE kritaflake_imagecodec
    PUBLIC
        pktextstream
        kritapigment
        kritacommand
        KF${KF_MAJOR}::WidgetsAddons
        Qt${QT_MAJOR_VERSION}::Svg
        KF${KF_MAJOR}::CoreAddons
        KF${KF_MAJOR}::ConfigCore
        KF${KF_MAJOR}::I18n
        Qt${QT_MAJOR_VERSION}::Gui
        Qt${QT_MAJOR_VERSION}::Xml
        # S-08 Task 8（判据⑨）：pk 库显式链入主树 kritaflake（与 kritaflake_OBJECT_DEPS
        # 同集），让主树链接层可见 pk 符号。pk 库 target 由根 CMakeLists.txt:1642 的
        # add_subdirectory(pk) 定义，此处只接线。
        pkstring pkcontainer pkcolor pkgeometry pkvariant pkxml pkimage pkport pkpointer
        pkconcurrent pklog pksignal pktime pkconfig pkflags pknamespace
        pkrender pkfont pkinput
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
