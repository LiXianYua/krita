set(kritalibpaintop_LIB_SRCS
    KisPaintopPropertiesBase.cpp
    KisMaskingBrushCompositeOpFactory.cpp
    kis_brush_based_paintop.cpp
    kis_brush_option.cpp
    KisColorOptionData.cpp
    kis_color_source.cpp
    kis_color_source_option.cpp
    KisCurveOptionDataUniformProperty.cpp
    KisSensorData.cpp
    KisSensorPackInterface.cpp
    KisKritaSensorPack.cpp
    KisCurveOptionDataCommon.cpp
    KisCurveOptionData.cpp
    KisCurveOption.cpp
    KisSpacingOptionData.cpp
    KisSpacingOption.cpp
    KisMirrorOptionData.cpp
    KisMirrorOption.cpp
    KisSharpnessOptionData.cpp
    KisSharpnessOption.cpp
    KisScatterOptionData.cpp
    KisScatterOption.cpp
    KisAirbrushOptionData.cpp
    KisPaintingModeOptionData.cpp
    KisColorSourceOptionData.cpp
    KisTextureOptionData.cpp
    KisEmbeddedTextureData.cpp
    KisSizeOptionData.cpp
    KisStandardOptions.cpp
    KisRotationOption.cpp
    KisOpacityOption.cpp
    KisFlowOpacityOption.cpp
    KisDarkenOption.cpp
    KisHSVOption.cpp
    KisFilterOptionData.cpp
    KisDabCacheUtils.cpp
    kis_dab_cache_base.cpp
    kis_dab_cache.cpp
    kis_precision_option.cpp
    kis_current_outline_fetcher.cpp
    kis_brush_based_paintop_settings.cpp
    kis_texture_option.cpp
    KisTextureMaskInfo.cpp
    kis_figure_painting_tool_helper.cpp
    KisAsynchronousStrokeUpdateHelper.cpp
    KisStrokeCompatibilityInfo.cpp
    KisStrokeSpeedMonitor.cpp
    strokes/freehand_stroke.cpp
    strokes/KisStrokeEfficiencyMeasurer.cpp
    strokes/kis_painter_based_stroke_strategy.cpp
    strokes/KisFreehandStrokeInfo.cpp
    strokes/KisMaskedFreehandStrokePainter.cpp
    strokes/KisMaskingBrushRenderer.cpp
    sensors/KisDynamicSensor.cpp
    sensors/KisDynamicSensors.cpp
    sensors/KisDynamicSensorDrawingAngle.cpp
    sensors/KisDynamicSensorFuzzy.cpp
    sensors/KisDynamicSensorDistance.cpp
    sensors/KisDynamicSensorFade.cpp
    sensors/KisDynamicSensorTime.cpp
)

kis_add_library(kritalibpaintop SHARED ${kritalibpaintop_LIB_SRCS} )
generate_export_header(kritalibpaintop BASE_NAME kritapaintop EXPORT_MACRO_NAME PAINTOP_EXPORT)

target_include_directories(kritalibpaintop
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        # 判据⑧：pk include 三方核对——源码 include 的每个 pk/<子目录>/ 头
        # （PkString/PkVector/PkRect/PkPen/PkPainterPath/PkSize/PkScopedPointer/
        # PkSharedPointer/PkObject/PkTimer/PkThread/PkMutex/PkElapsedTimer/PkNodeId/
        # PkFlags/PkAuxTypes/PkXmlDocument/PkXmlElement/PkGlobal），薄壳
        # SHELL_INCLUDE_DIRS 与主树此处都有。compat 垫片目录（pk/*/compat）是薄壳专属
        # 的 Q→Pk 宏映射，主树不加，照 S-06 libs/image 先例。顺序照薄壳
        # （pk/global 须在 pk/geometry 前：PkGlobal.h 两处同名）。
        ${CMAKE_SOURCE_DIR}/pk/string
        ${CMAKE_SOURCE_DIR}/pk/container
        ${CMAKE_SOURCE_DIR}/pk/time
        ${CMAKE_SOURCE_DIR}/pk/log
        ${CMAKE_SOURCE_DIR}/pk/signal
        ${CMAKE_SOURCE_DIR}/pk/pointer
        ${CMAKE_SOURCE_DIR}/pk/config
        ${CMAKE_SOURCE_DIR}/pk/global
        ${CMAKE_SOURCE_DIR}/pk/color
        ${CMAKE_SOURCE_DIR}/pk/geometry
        ${CMAKE_SOURCE_DIR}/pk/image
        ${CMAKE_SOURCE_DIR}/pk/xml
        ${CMAKE_SOURCE_DIR}/pk/port
        ${CMAKE_SOURCE_DIR}/pk/concurrent
        ${CMAKE_SOURCE_DIR}/pk/flags
        ${CMAKE_SOURCE_DIR}/pk/variant
        ${CMAKE_SOURCE_DIR}/pk/namespace
        ${CMAKE_SOURCE_DIR}/pk/uuid
)

target_link_libraries(kritalibpaintop
    PUBLIC
        kritalibbrush
        kritaimage
        # 判据⑨：pk 库接入主构建。PUBLIC——libpaintop 公开头引用 Pk 类型，下游消费方
        # 需经此传递获得 pk 头目录与链接。与薄壳 SHELL_LINK_LIBRARIES 同集（16 库）。
        # 注意：主树整链被 libs/brush（S-07-a 锁，未剥）阻塞，此声明为准备性——
        # 薄壳是当前唯一验证方式（决策 D-19）。
        pkstring
        pkcontainer
        pkgeometry
        pkxml
        pkpointer
        pkconcurrent
        pktime
        pkcolor
        pkimage
        pkvariant
        pksignal
        pklog
        pkglobal
        pkuuid
        pkport
        pkflags
)

set_target_properties(kritalibpaintop PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritalibpaintop  ${INSTALL_TARGETS_DEFAULT_ARGS})


add_subdirectory(tests)
