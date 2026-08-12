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
#
# ---------------------------------------------------------------------------
# 交接：被删掉的 krita target 是全树 7 个 .qrc 的唯一编译点
# ---------------------------------------------------------------------------
# 972ab5d 时 krita/CMakeLists.txt 有个 krita_QRCS 列表（原第 112-138 行），
# 它编译的 qrc 不止 krita/ 自己的，还包括保留侧的。krita target 一删，
# 下面这些 qrc 的文件都还在，但**没有任何 target 再编译它们**：
#
#   plugins/paintops/defaultpresets/defaultpresets.qrc  18 个 .kpp 默认笔刷预设
#                                   （该目录连 CMakeLists.txt 都没有，基线即如此，
#                                     它全部的构建参与就是被 krita_QRCS 引用这一处）
#   pics/icons.qrc                   245 条目
#   libs/flake/flake.qrc              16 条目
#   libs/widgets/kritawidgets.qrc     16 条目
#   krita/data/shaders/shaders.qrc    15 条目（OpenGL 画布着色器）
#   krita/data/gamutmasks/gamutmasks.qrc   2 条目
#   krita/data/mime-database/mime-database.qrc  1 条目（Qt6 分支才加）
#
# **现在没有功能回归**：这些资源原本也只存在于 krita 可执行文件里，那个文件
# 已经不存在了；全部 23 个存活 qrc 的内部路径引用零断链，ctest 全绿。
#
# 但这是编译器看不见的状态——qrc 没人编不会报错。**建新入口点（F 线接 Flutter，
# 或任何重新引入可执行 target 的任务）时必须重新声明这批 qrc**，否则默认笔刷
# 预设和画布着色器会静默缺失。

add_subdirectory(data)
