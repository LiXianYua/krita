# --- R-80 T3：paintop 插件「静态全局对象自注册」的强制链入 -------------------
# D-12 之后插件是 STATIC 库，注册靠翻译单元里的静态全局对象完成：
#   plugins/paintops/colorsmudge/colorsmudge_paintop_plugin.cpp:31
#       static ColorSmudgePaintOpRegistration s_colorSmudgePaintOpRegistration;
# 链接器只按**符号引用**从归档里取成员，`colorsmudge_paintop_plugin.cpp.o` 里除了
# 那个内部链接的静态对象之外没有别的可被引用的符号，于是整份被丢掉、构造函数不运行、
# `KisPaintOpRegistry` 里没有 "colorsmudge"。
#
# 这一条同时打断两个东西，都在本次实测的日志里可见：
#   ① 预设载入失败 —— libs/image/brushengine/kis_paintop_preset.cpp:559
#          if (KisPaintOpRegistry::instance()->get(paintopid) == 0) { setValid(false); }
#      ⇒ `KoResource.cpp:92 Could not load resource file ".../test_smudge_*.kpp"`；
#   ② 笔画画不出东西 —— 预设无效 ⇒ NDEBUG 下 `Q_ASSERT(presetValid)`
#      （sdk/tests/stroke_testing_utils.cpp:109）空转 ⇒ 落笔为空 ⇒
#      `checkOneLayer(image, targetNode, testPrefix)` 逐行 FALSE ⇒ abort。
#
# 形制与 libs/image/tests/CMakeLists.txt 的 KRITA_BLURFILTER_WHOLE_ARCHIVE 一致：
# Mach-O 用 `-Wl,-force_load,<归档绝对路径>`（$<TARGET_FILE:> 给绝对路径），
# ELF 用 `-Wl,--whole-archive … -Wl,--no-whole-archive`。
# 注意**只窄链这一个插件**：不把 plugins/kritaplugin_registry 聚合器链进来
# （R-77 plans/R-77.md §3.1.3 已裁定反向——聚合器会把全部插件、含 Qt 头面拖进来）。
if(APPLE)
    set(KRITA_COLORSMUDGE_WHOLE_ARCHIVE "-Wl,-force_load,$<TARGET_FILE:kritacolorsmudgepaintop>")
else()
    set(KRITA_COLORSMUDGE_WHOLE_ARCHIVE
        "-Wl,--whole-archive" "$<TARGET_FILE:kritacolorsmudgepaintop>" "-Wl,--no-whole-archive")
endif()

kis_add_test(
    KisColorsmudgeOpTest.cpp
     $<TARGET_PROPERTY:kritatestsdk,SOURCE_DIR>/stroke_testing_utils.cpp
    TEST_NAME KisColorsmudgeOpTest
    LINK_LIBRARIES kritalibpaintop kritaimage kritatestsdk
                   kritacolorsmudgepaintop ${KRITA_COLORSMUDGE_WHOLE_ARCHIVE}
    )
