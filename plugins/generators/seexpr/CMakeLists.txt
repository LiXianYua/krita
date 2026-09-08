set(kritaseexprgenerator_SOURCES
    SeExprVariable.cpp
    SeExprExpressionContext.cpp
    generator.cpp
    )

kis_add_library(kritaseexprgenerator STATIC ${kritaseexprgenerator_SOURCES})
target_compile_definitions(kritaseexprgenerator PRIVATE QT_NO_VERSION_TAGGING)
set_target_properties(kritaseexprgenerator PROPERTIES AUTOMOC OFF)

target_link_libraries(kritaseexprgenerator PRIVATE
    kritaimage KSeExpr::KSeExpr
    pkcontainer pkstring pkvariant pkgeometry
)

install(TARGETS kritaseexprgenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

add_subdirectory(tests)
