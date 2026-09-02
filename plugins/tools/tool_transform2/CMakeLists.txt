add_subdirectory(tests)

set(kritatooltransform_static_SRCS
    tool_transform_args.cc
    kis_transform_mask_adapter.cpp
    KisAnimatedTransformMaskParamsHolder.cpp
    kis_tool_transform.cc
    kis_transform_strategy_base.cpp
    kis_warp_transform_strategy.cpp
    kis_cage_transform_strategy.cpp
    kis_simplified_action_policy_strategy.cpp
    kis_liquify_transform_strategy.cpp
    kis_liquify_paint_helper.cpp
    kis_liquify_paintop.cpp
    kis_liquify_properties.cpp
    kis_mesh_transform_strategy.cpp
    kis_free_transform_strategy.cpp
    kis_free_transform_strategy_gsl_helpers.cpp
    kis_perspective_transform_strategy.cpp
    kis_transform_utils.cpp
    strokes/transform_stroke_strategy.cpp
    strokes/inplace_transform_stroke_strategy.cpp
    TransformToolPlatform.cpp
    TransformToolRegistration.cpp
    )

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(kritatooltransform_SIGNAL_HEADERS
    kis_warp_transform_strategy.h
    kis_liquify_transform_strategy.h
    kis_mesh_transform_strategy.h
    kis_free_transform_strategy.h
    kis_perspective_transform_strategy.h
    kis_tool_transform.h
    strokes/transform_stroke_strategy.h
    strokes/inplace_transform_stroke_strategy.h)
set(kritatooltransform_SIGNAL_SRCS)
foreach(signal_header IN LISTS kritatooltransform_SIGNAL_HEADERS)
    get_filename_component(signal_name "${signal_header}" NAME_WE)
    set(signal_source "${CMAKE_CURRENT_BINARY_DIR}/${signal_name}_pk_signals.cpp")
    add_custom_command(
        OUTPUT "${signal_source}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
                -o "${signal_source}"
        DEPENDS
                "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
        COMMENT "pk_signal_moc: generating ${signal_name} signal definitions"
        VERBATIM)
    list(APPEND kritatooltransform_SIGNAL_SRCS "${signal_source}")
endforeach()

list(APPEND kritatooltransform_static_SRCS ${kritatooltransform_SIGNAL_SRCS})

kis_add_library(kritatooltransform_static STATIC ${kritatooltransform_static_SRCS})
generate_export_header(kritatooltransform_static BASE_NAME kritatooltransform)

set(kritatooltransform_SOURCES
  tool_transform.cc
)

kis_add_library(kritatooltransform MODULE ${kritatooltransform_SOURCES})

set_target_properties(kritatooltransform_static kritatooltransform PROPERTIES
    AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)

set(kritatooltransform_PK_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/libs/pigment
    ${CMAKE_SOURCE_DIR}/pk/concurrent
    ${CMAKE_SOURCE_DIR}/pk/config
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/global
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/log
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/time
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/xml)
target_include_directories(kritatooltransform_static PRIVATE ${kritatooltransform_PK_INCLUDE_DIRS})
target_include_directories(kritatooltransform PRIVATE ${kritatooltransform_PK_INCLUDE_DIRS})

target_link_libraries(kritatooltransform_static
  PUBLIC
    kritacanvas
    kritashapemodel
    pkconcurrent
    pkconfig
    pkcontainer
    pkgeometry
    pkglobal
    pkimage
    pklog
    pkpointer
    pkrender
    pksignal
    pkstring
    pktime
    pkvariant
    pkxml
)

if (NOT GSL_FOUND)
  message (WARNING "KRITA WARNING! No GNU Scientific Library was found! Krita's Transform Tool will not be able to scale the image with handles. Please install GSL library.")
else ()
  target_link_libraries(kritatooltransform_static
    PUBLIC
      GSL::gsl
  )
endif ()

target_link_libraries(kritatooltransform PRIVATE kritatooltransform_static)

install(TARGETS kritatooltransform  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})


install( FILES
    KisToolTransform.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
