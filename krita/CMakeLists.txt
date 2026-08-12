# 应用层已由 D-01 删除（§6.1 D1）：krita/krita_version/krita_windows_stub_* 等
# target、pics/、integration/、dtd/ 以及 13 个 UI 资产子目录都不在了。
#
# 这个目录仍然存在，只有两个原因：
#
#   1. krita/data/ 是测试 SDK 的资源根 —— sdk/tests/CMakeLists.txt 把
#      SYSTEM_RESOURCES_DATA_DIR 指到 ${CMAKE_SOURCE_DIR}/krita/data/，
#      kis_kra_saver_test / KisBrushOpTest / TestBundleStorage 三个保留 target
#      的测试直接读它。搬走要改 sdk/tests，那是 S-00 的锁。
#   2. krita/krita.qrc（连同 krita5.xmlgui 与 data/kritarc）被
#      libs/ui/tests/kis_view_signals_test 编译，归 D-03。
#
# 两件事都做完之后，本目录连同 data/ 应搬到顶层并彻底消失。

add_subdirectory(data)
