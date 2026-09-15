add_subdirectory(tests)

set(kritaqmlexport_SOURCES
    qml_format.cc
    qml_converter.cc
    qml_export.cc
    )

kis_add_library(kritaqmlexport STATIC ${kritaqmlexport_SOURCES})

# qml_format.cc 用 PkTextStream（pk/textstream），必须链它的实现库；
# 先前只链 kritaimpex/kritastore/kritapigment，链接期 PkTextStream 的
# 构造/析构未定义（在 kis_impex_static_registration_test 上暴露）。
target_link_libraries(kritaqmlexport kritaimpex kritastore kritapigment pktextstream)

install(TARGETS kritaqmlexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
