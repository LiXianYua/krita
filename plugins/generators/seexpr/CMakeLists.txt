set(kritaseexprgenerator_SOURCES
    SeExprVariable.cpp
    SeExprExpressionContext.cpp
    generator.cpp
    )

kis_add_library(kritaseexprgenerator MODULE ${kritaseexprgenerator_SOURCES})
set_target_properties(kritaseexprgenerator PROPERTIES AUTOMOC OFF)

target_link_libraries(kritaseexprgenerator PRIVATE
    kritaimage KSeExpr::KSeExpr
    pkcontainer pkstring pkvariant pkgeometry
)

install(TARGETS kritaseexprgenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

add_subdirectory(tests)
