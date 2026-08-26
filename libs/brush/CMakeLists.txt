add_subdirectory( tests )

set(kritalibbrush_LIB_SRCS
    kis_predefined_brush_factory.cpp
    kis_auto_brush.cpp
    kis_boundary.cc
    kis_brush.cpp
    kis_scaling_size_brush.cpp
    kis_brush_registry.cpp
    KisBrushServerProvider.cpp
    kis_gbr_brush.cpp
    kis_abr_brush.cpp
    kis_abr_brush_collection.cpp
    kis_imagepipe_brush.cpp
    kis_pipebrush_parasite.cpp
    kis_png_brush.cpp
    kis_svg_brush.cpp
    kis_qimage_pyramid.cpp
    kis_text_brush.cpp
    kis_auto_brush_factory.cpp
    kis_text_brush_factory.cpp
    KisAbrStorage.cpp
    KisColorfulBrush.cpp
    KisBrushTypeMetaDataFixup.cpp
    KisBrushModel.cpp
)

include_directories(
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/xml
    ${CMAKE_SOURCE_DIR}/pk/port
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/concurrent
    ${CMAKE_SOURCE_DIR}/pk/time
    ${CMAKE_SOURCE_DIR}/pk/sql
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/global
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/namespace
    ${CMAKE_SOURCE_DIR}/pk/uuid
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/log
)

kis_add_library(kritalibbrush SHARED ${kritalibbrush_LIB_SRCS})
generate_export_header(kritalibbrush BASE_NAME kritabrush EXPORT_MACRO_NAME BRUSH_EXPORT)

target_link_libraries(kritalibbrush kritaimage Qt${QT_MAJOR_VERSION}::Svg kritamultiarch lager)

set_target_properties(kritalibbrush PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritalibbrush  ${INSTALL_TARGETS_DEFAULT_ARGS})

