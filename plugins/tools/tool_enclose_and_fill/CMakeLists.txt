set(kritatoolencloseandfill_SOURCES
    KisToolEncloseAndFill.cpp
    KisToolEncloseAndFillPlugin.cpp
    subtools/KisRectangleEnclosingProducer.cpp
    subtools/KisEllipseEnclosingProducer.cpp
    subtools/KisPathEnclosingProducer.cpp
    subtools/KisLassoEnclosingProducer.cpp
    subtools/KisToolBasicBrushBase.cpp
    subtools/KisBrushEnclosingProducer.cpp
    )

kis_add_library(kritatoolencloseandfill MODULE ${kritatoolencloseandfill_SOURCES})

set_target_properties(kritatoolencloseandfill PROPERTIES
    AUTOMOC OFF CXX_COMPILER_LAUNCHER ccache)
target_include_directories(kritatoolencloseandfill PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/render
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/xml)

target_link_libraries(kritatoolencloseandfill
    kritacanvas
    kritashapemodel
    kritaflake
    kritaimage
    pkcolor
    pkcontainer
    pkgeometry
    pkpointer
    pkrender
    pksignal
    pkstring
    pkvariant
    pkxml)

install(TARGETS kritatoolencloseandfill  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

install( FILES KisToolEncloseAndFill.action DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions)
