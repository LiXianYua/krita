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
)

target_link_libraries(kritalibpaintop
    PUBLIC
        kritalibbrush
        kritaimage
    PRIVATE
        kritawidgets
        kritawidgetutils
)

set_target_properties(kritalibpaintop PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritalibpaintop  ${INSTALL_TARGETS_DEFAULT_ARGS})


add_subdirectory(tests)
