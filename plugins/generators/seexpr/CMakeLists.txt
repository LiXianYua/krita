set(kritaseexprgenerator_SOURCES
    SeExprVariable.cpp
    SeExprExpressionContext.cpp
    generator.cpp
    )

kis_add_library(kritaseexprgenerator STATIC ${kritaseexprgenerator_SOURCES})
set_target_properties(kritaseexprgenerator PROPERTIES AUTOMOC OFF)

target_link_libraries(kritaseexprgenerator PRIVATE
    kritaimage_noqt_compile_interface
    KSeExpr::KSeExpr
    pkcontainer pkstring pkvariant pkgeometry
)
target_link_libraries(kritaseexprgenerator INTERFACE
    $<LINK_ONLY:kritaimage>)
add_dependencies(kritaseexprgenerator kritaimage)

install(TARGETS kritaseexprgenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

add_subdirectory(tests)
