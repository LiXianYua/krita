set(kritalibpaintop_LIB_SRCS
    kis_brush_based_paintop.cpp
    KisAutoBrushModel.cpp
    KisPredefinedBrushModel.cpp
    kis_brush_option.cpp
    KisColorOptionData.cpp
    KisColorOptionModel.cpp
    kis_color_source.cpp
    kis_color_source_option.cpp
    KisCurveOptionDataUniformProperty.cpp
    KisSensorData.cpp
    KisSensorPackInterface.cpp
    KisKritaSensorPack.cpp
    KisCurveOptionDataCommon.cpp
    KisCurveOptionData.cpp
    KisCurveOptionInputControlsStrategyInterface.cpp
    KisCurveOptionInputControlsStrategy.cpp
    KisCurveOptionRangeControlsStrategyInterface.cpp
    KisCurveRangeModelInterface.cpp
    KisCurveRangeModel.cpp
    KisCurveOptionModel.cpp
    KisCurveOption.cpp
    KisMultiSensorsModel.cpp
    KisDynamicSensorFactory.cpp
    KisDynamicSensorFactoryRegistry.cpp
    KisSimpleDynamicSensorFactory.cpp
    KisSpacingOptionData.cpp
    KisSpacingOptionModel.cpp
    KisSpacingOption.cpp
    KisMirrorOptionData.cpp
    KisMirrorOptionModel.cpp
    KisMirrorOption.cpp
    KisSharpnessOptionData.cpp
    KisSharpnessOptionModel.cpp
    KisSharpnessOption.cpp
    KisScatterOptionData.cpp
    KisScatterOptionModel.cpp
    KisScatterOption.cpp
    KisAirbrushOptionData.cpp
    KisAirbrushOptionModel.cpp
    KisCompositeOpOptionData.cpp
    KisCompositeOpOptionModel.cpp
    KisPaintingModeOptionData.cpp
    KisPaintingModeOptionModel.cpp
    KisColorSourceOptionData.cpp
    KisColorSourceOptionModel.cpp
    KisTextureOptionData.cpp
    KisEmbeddedTextureData.cpp
    KisTextureOptionModel.cpp
    KisSizeOptionData.cpp
    KisStandardOptionData.cpp
    KisStandardOptions.cpp
    KisRotationOption.cpp
    KisOpacityOption.cpp
    KisFlowOpacityOption.cpp
    KisDarkenOption.cpp
    KisHSVOption.cpp
    KisFilterOptionData.cpp
    KisFilterOptionModel.cpp
    KisDabCacheUtils.cpp
    kis_dab_cache_base.cpp
    kis_dab_cache.cpp
    kis_precision_option.cpp
    kis_current_outline_fetcher.cpp
    kis_brush_based_paintop_settings.cpp
    kis_texture_option.cpp
    KisTextureMaskInfo.cpp
    KisMaskingBrushOption.cpp
    KisMaskingBrushOptionProperties.cpp
    sensors/KisDynamicSensor.cpp
    sensors/KisDynamicSensors.cpp
    sensors/KisDynamicSensorDrawingAngle.cpp
    sensors/KisDynamicSensorFuzzy.cpp
    sensors/KisDynamicSensorDistance.cpp
    sensors/KisDynamicSensorFade.cpp
    sensors/KisDynamicSensorTime.cpp
    KisDynamicSensorFactoryTime.cpp
    KisDynamicSensorFactoryFade.cpp
    KisDynamicSensorFactoryDistance.cpp
    KisDynamicSensorFactoryDrawingAngle.cpp
    KisSensorWithLengthModel.cpp
    KisDrawingAngleSensorModel.cpp
    KisTextBrushModel.cpp
)

ki18n_wrap_ui(kritalibpaintop_LIB_SRCS
    sensors/SensorDistanceConfiguration.ui
    sensors/SensorTimeConfiguration.ui
    sensors/SensorFadeConfiguration.ui

)

kis_add_library(kritalibpaintop SHARED ${kritalibpaintop_LIB_SRCS} )
generate_export_header(kritalibpaintop BASE_NAME kritapaintop EXPORT_MACRO_NAME PAINTOP_EXPORT)

target_link_libraries(kritalibpaintop kritaui kritalibbrush kritawidgetutils)
target_link_libraries(kritalibpaintop LINK_INTERFACE_LIBRARIES kritaui kritalibbrush)

set_target_properties(kritalibpaintop PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritalibpaintop  ${INSTALL_TARGETS_DEFAULT_ARGS})


add_subdirectory(tests)
