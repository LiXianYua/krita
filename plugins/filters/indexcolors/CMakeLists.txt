set(INDEXCOLORS_SOURCE indexcolors.cpp indexcolorpalette.cpp palettegeneratorconfig.cpp)
kis_add_library(kritaindexcolors MODULE ${INDEXCOLORS_SOURCE})
# 判据⑧⑨（S-09-b Task5）：pk 头经 kritaimage→kritapigment 的 PUBLIC 传递已可达，
# 这里把 indexcolors 直接消费的 pk 模块显式声明白（pk/variant 的 PkDataStream、
# pk/color 的 PkColor、pk/geometry 的 PkRect、pk/container 的 PkVector/PkPair）。
target_link_libraries(kritaindexcolors PRIVATE
    kritaimage
    pkcolor
    pkvariant
    pkgeometry
    pkstring
    pkcontainer)
install(TARGETS kritaindexcolors          DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
