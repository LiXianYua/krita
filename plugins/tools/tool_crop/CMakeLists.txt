set(kritatoolcrop_SOURCES
    tool_crop.cc
    kis_tool_crop.cc
    kis_constrained_rect.cpp
    )

kis_add_library(kritatoolcrop STATIC ${kritatoolcrop_SOURCES})

set_target_properties(kritatoolcrop PROPERTIES AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(kritatoolcrop PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/config
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/variant)

target_link_libraries(kritatoolcrop
    kritacanvas
    kritashapemodel
    pkcontainer
    pkconfig
    pkgeometry
    pkpointer
    pkrender
    pksignal
    pkstring
    pkvariant
)

install(TARGETS kritatoolcrop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install( FILES
    KisToolCrop.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
