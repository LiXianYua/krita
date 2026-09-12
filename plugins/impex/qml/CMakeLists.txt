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
# 两处同名头 `PkTextStream.h`（pk/textstream/ 与 libs/pigment/）；pigment 那份是
# S-03-a 的 out-of-line 垫片、其符号在 libkritapigment 里是隐藏的，跨库不可解析。
# qml_format.cc 要的是 pk 版（PkStream 后端、inline 定义），把它的头目录放到最前，
# 避免解析到 pigment 那份。
target_include_directories(kritaqmlexport BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/pk/textstream)

install(TARGETS kritaqmlexport  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
