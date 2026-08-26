add_subdirectory(tests)

set(kritalibkra_LIB_SRCS
    kis_colorize_dom_utils.cpp
    kis_colorize_dom_utils.h
    kis_kra_loader.cpp
    kis_kra_loader.h
    kis_kra_load_visitor.cpp
    kis_kra_load_visitor.h
    kis_kra_saver.cpp
    kis_kra_saver.h
    kis_kra_save_visitor.cpp
    kis_kra_save_visitor.h
    kis_kra_savexml_visitor.cpp
    kis_kra_savexml_visitor.h
    kis_kra_tags.h
    kis_kra_utils.cpp
    kis_kra_utils.h
    kra_converter.cpp
)

kis_add_library(kritalibkra SHARED ${kritalibkra_LIB_SRCS})
target_link_libraries(kritalibkra kritaimpex kritaimage ZLIB::ZLIB)
# 判据⑧：pk include 对齐。libkra 源码 include 的 pk 头 → 主树声明一致。
# PkNodeId.h 在 pk/uuid（S-05-b 补定义）；其余 pk 头经 kritaimpex/kritaimage
# 的链接闭包传递（pk/string pk/xml pk/signal pk/variant 等）。
target_include_directories(kritalibkra PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/uuid
)
generate_export_header(kritalibkra BASE_NAME kritalibkra)

set_target_properties(kritalibkra PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritalibkra ${INSTALL_TARGETS_DEFAULT_ARGS} )
