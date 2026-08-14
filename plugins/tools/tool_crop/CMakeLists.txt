set(kritatoolcrop_SOURCES
    tool_crop.cc
    kis_tool_crop.cc
    kis_constrained_rect.cpp
    )
qt_add_resources(kritatoolcrop_SOURCES toolcrop.qrc)

kis_add_library(kritatoolcrop MODULE ${kritatoolcrop_SOURCES})

target_link_libraries(kritatoolcrop
    kritacanvas
    kritashapemodel
)

install(TARGETS kritatoolcrop  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install( FILES
    KisToolCrop.action
DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
