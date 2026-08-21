project(kritapigment)

ko_compile_for_all_implementations_no_scalar(__per_arch_factory_objs compositeops/KoOptimizedCompositeOpFactoryPerArch.cpp)
ko_compile_for_all_implementations(__per_arch_alpha_applicator_factory_objs KoAlphaMaskApplicatorFactoryImpl.cpp)
ko_compile_for_all_implementations(__per_arch_rgb_scaler_factory_objs KoOptimizedPixelDataScalerU8ToU16FactoryImpl.cpp)

message("Following objects are generated from the per-arch lib")
foreach(_obj IN LISTS __per_arch_factory_objs __per_arch_alpha_applicator_factory_objs __per_arch_rgb_scaler_factory_objs)
    message("    * ${_obj}")
endforeach()

add_subdirectory(tests)
add_subdirectory(benchmarks)

set(kritapigment_SRCS
    DebugPigment.cpp
    KoBasicHistogramProducers.cpp
    KoAlphaMaskApplicatorBase.cpp
    KoOptimizedPixelDataScalerU8ToU16Base.cpp
    KoOptimizedPixelDataScalerU8ToU16Factory.cpp
    KoColor.cpp
    KoColorDisplayRendererInterface.cpp
    KoColorConversionAlphaTransformation.cpp
    KoColorConversionCache.cpp
    KoColorConversions.cpp
    KoColorConversionSystem.cpp
    KoColorConversionTransformation.cpp
    KoColorProofingConversionTransformation.cpp
    KoColorConversionTransformationFactory.cpp
    KoColorModelStandardIds.cpp
    KoColorProfile.cpp
    KoColorSpace.cpp
    KoColorSpaceEngine.cpp
    KoColorSpaceFactory.cpp
    KoColorSpaceMaths.cpp
    KoCmykColorSpaceMaths.cpp
    KoLabColorSpaceMaths.cpp
    KoColorSpaceRegistry.cpp
    KoColorProfileStorage.cpp
    KoColorTransformation.cpp
    KoColorTransformationFactory.cpp
    KoColorTransformationFactoryRegistry.cpp
    KoCompositeColorTransformation.cpp
    KoCompositeOp.cpp
    KoCompositeOpRegistry.cpp
    KoCopyColorConversionTransformation.cpp
    KoFallBackColorTransformation.cpp
    KoHistogramProducer.cpp
    KoMultipleColorConversionTransformation.cpp
    colorspaces/KoAlphaColorSpace.cpp
    colorspaces/KoLabColorSpace.cpp
    colorspaces/KoRgbU16ColorSpace.cpp
    colorspaces/KoRgbU8ColorSpace.cpp
    colorspaces/KoSimpleColorSpaceEngine.cpp
    compositeops/KoOptimizedCompositeOpFactory.cpp
    compositeops/KoOptimizedCompositeOpFactoryPerArch_Scalar.cpp
    compositeops/KoAlphaDarkenParamsWrapper.cpp
    compositeops/KoColorSpaceBlendingPolicy.cpp
    ${__per_arch_factory_objs}
    ${__per_arch_alpha_applicator_factory_objs}
    ${__per_arch_rgb_scaler_factory_objs}
    KoAlphaMaskApplicatorFactory.cpp
    colorprofiles/KoDummyColorProfile.cpp
    resources/KoAbstractGradient.cpp
    resources/KoColorSet.cpp
    resources/KisSwatch.cpp
    resources/KisSwatchGroup.cpp
    resources/KoPattern.cpp
    resources/KoStopGradient.cpp
    resources/KoSegmentGradient.cpp
    resources/KoCachedGradient.h
    resources/KisGradientConversion.cpp
    resources/KisUniqueColorSet.cpp
)

if(MSVC OR (WIN32 AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel"))
  # avoid "cannot open file 'LIBC.lib'" error
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /NODEFAULTLIB:LIBC.LIB")
endif()

kis_add_library(kritapigment SHARED ${kritapigment_SRCS})

generate_export_header(kritapigment)

target_include_directories( kritapigment
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/resources>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/compositeops>
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/string
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/container
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/variant
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/port
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/pointer
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/geometry
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/global
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/log
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/concurrent
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/image
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/time
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/flags
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/color
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/signal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/sql
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/xml
    ${CMAKE_CURRENT_SOURCE_DIR}/../../pk/namespace
)

target_link_libraries( kritapigment
    PUBLIC
        kritaplugin
        kritastore
        kritaglobal
        kritaresources
        kritacommand
        ${LINK_OPENEXR_LIB}
        kritamultiarch
        Boost::boost
        pkstring
        pkcontainer
        pkcolor
        pkgeometry
        pkvariant
        pkxml
        pkimage
        pkport
        pkpointer
        pkconcurrent
        pklog
        pksignal
        pktime
        pkconfig
        pkflags
        pknamespace
)

set_target_properties(kritapigment PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritapigment  ${INSTALL_TARGETS_DEFAULT_ARGS})
