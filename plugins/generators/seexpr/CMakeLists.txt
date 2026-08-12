set(kritaseexprgenerator_SOURCES
    SeExprVariable.cpp
    SeExprExpressionContext.cpp
    generator.cpp
    )

kis_add_library(kritaseexprgenerator MODULE ${kritaseexprgenerator_SOURCES})

target_link_libraries(kritaseexprgenerator kritaimage KSeExpr::KSeExpr)

install(TARGETS kritaseexprgenerator  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

add_subdirectory(tests)
