# cmake in some versions for some not yet known reasons fails to run automoc
# on random targets (changing target names already has an effect)
# As temporary workaround skipping build of tests on these versions for now
# See https://mail.kde.org/pipermail/kde-buildsystem/2015-June/010819.html
# extend range of affected cmake versions as needed
if(NOT ${CMAKE_VERSION} VERSION_LESS 3.1.3 AND
   NOT ${CMAKE_VERSION} VERSION_GREATER 3.2.3)
    message(WARNING "Skipping krita/image/tests, CMake in at least versions 3.1.3 - 3.2.3 seems to have a problem with automoc.  \n(FRIENDLY REMINDER: PLEASE DON'T BREAK THE TESTS!)")
    set (HAVE_FAILING_CMAKE TRUE)
else()
    set (HAVE_FAILING_CMAKE FALSE)
endif()

set(KisRandomGenerator2DDemoSources KisRandomGenerator2DDemo.cpp kimageframe.cpp)
ki18n_wrap_ui(KisRandomGenerator2DDemoSources KisRandomGenerator2DDemo.ui)
add_executable(KisRandomGenerator2DDemo ${KisRandomGenerator2DDemoSources})
target_link_libraries(KisRandomGenerator2DDemo kritaimage)
ecm_mark_as_test(KisRandomGenerator2DDemo)

add_executable(KisPathRasterizerQtOracle
    KisPathRasterizerQtOracle.cpp
    path_rasterizer_cases.h)
target_link_libraries(KisPathRasterizerQtOracle PRIVATE Qt5::Gui)
ecm_mark_as_test(KisPathRasterizerQtOracle)

add_executable(KisPathRasterizerPkProbe
    KisPathRasterizerPkProbe.cpp
    ../private/kis_path_rasterizer.cpp
    ../private/kis_path_scan_converter.cpp
    ../private/kis_cosmetic_stroker.cpp
    path_rasterizer_cases.h)
target_link_libraries(KisPathRasterizerPkProbe PRIVATE pkgeometry pkcolor pkrender)
ecm_mark_as_test(KisPathRasterizerPkProbe)

# --no-as-needed 是 ELF 链接器的说法。Mach-O 默认就把命令行上列出的每个 dylib
# 都记进 LC_LOAD_DYLIB（没有 ELF 的 --as-needed 语义），所以这两个开关在 Apple
# 工具链上既无效（clang 报 `unknown options`）也不需要——按平台置空即可。
if(APPLE)
    set(KRITA_LCMS_NO_AS_NEEDED "")
    set(KRITA_AS_NEEDED "")
else()
    set(KRITA_LCMS_NO_AS_NEEDED "-Wl,--no-as-needed")
    set(KRITA_AS_NEEDED "-Wl,--as-needed")
endif()

# --whole-archive 同样是 GNU ld 的说法，但**语义与 --no-as-needed 不同**，不能照
# 上面的形制置空：--no-as-needed 在 Mach-O 上「本来就无需」（默认把命令行每个
# dylib 记进 LC_LOAD_DYLIB），置空是语义等价；--whole-archive 是「强制把归档里
# 全部成员拉进来，包括没被引用的」，Mach-O 的对应物是 -force_load,<归档绝对路径>
# （缺它则「静态全局构造自注册」的对象会被链接器丢弃、构造函数不运行）。路径必须
# 是绝对路径，$<TARGET_FILE:> 正好给出绝对路径；写错路径 ld 会直接报错（M5 实测
# 退出码 1）。用法见下方 kis_fill_painter_test。
if(APPLE)
    set(KRITA_LCMS_WHOLE_ARCHIVE "-Wl,-force_load,$<TARGET_FILE:kritalcmsengine>")
else()
    set(KRITA_LCMS_WHOLE_ARCHIVE
        "-Wl,--whole-archive" "$<TARGET_FILE:kritalcmsengine>" "-Wl,--no-whole-archive")
endif()

# --- R-80：插件「静态全局对象自注册」的强制链入 -----------------------------
# D-12 之后插件是 STATIC 库，多数靠翻译单元里的静态全局对象完成注册
# （例：plugins/filters/blur/blur.cpp 末尾的 s_blurFilterPluginFilterRegistration）。
# 链接器只按**符号引用**从归档里取成员，没有引用的那个 .o 会被整份丢掉，
# 于是全局构造函数不运行、注册不发生。这与上面 kritalcmsengine 是同一件事
# （kis_fill_painter_test 的注释已写明），处置方式也一样：-force_load /
# --whole-archive 强制包含。
#
# 为什么必须收：sdk/tests/qimage_based_test.h:90-92 是
#     KisFilterSP filter = KisFilterRegistry::instance()->value("blur");
#     Q_ASSERT(filter);
#     ... = filter->defaultConfiguration(...);
# Q_ASSERT 在 RelWithDebInfo（NDEBUG）下是空转的，filter 为 null 时直接空指针
# 解引用 —— 实测 SIGSEGV，backtrace 停在 qimage_based_test.h:92:58。
if(APPLE)
    set(KRITA_BLURFILTER_WHOLE_ARCHIVE "-Wl,-force_load,$<TARGET_FILE:kritablurfilter>")
else()
    set(KRITA_BLURFILTER_WHOLE_ARCHIVE
        "-Wl,--whole-archive" "$<TARGET_FILE:kritablurfilter>" "-Wl,--no-whole-archive")
endif()

# --- R-80：paintop 工厂的强制链入 ------------------------------------------
# 同一个机理的第三个实例。KisPaintOpPresetTest 读的 .kpp 里写着
# paintopid="paintbrush"（test-embedded-resources-2.2.kpp；2.2/5.0/bad-md5
# 三份都是同一个 paintop），而 KisPaintOpPreset::fromXML
# （libs/image/brushengine/kis_paintop_preset.cpp:559）在
#     if (KisPaintOpRegistry::instance()->get(paintopid) == 0) { setValid(false); ... }
# 处直接判预设非法。paintbrush 与 duplicate 两个工厂由
# plugins/paintops/defaultpaintops/defaultpaintops_plugin.cc 末尾的
#     static DefaultPaintOpsRegistration s_defaultPaintOpsRegistration;
# 自注册，而 kritadefaultpaintops 是只含那一个 .cc 的 STATIC 库 ——
# 无符号引用时整份被链接器丢弃，注册对象不构造，注册表为空。
# 实测（本 worktree）：修前 KisPaintOpPresetTest 2 passed / 5 failed
# （preset->valid() 在 KisPaintOpPresetTest.cpp:163 逐行 FALSE），
# 本段强制链入后 4 passed / 3 failed（×3 稳定）。
# 该测试另有两条独立缺口：一条由下方 TESTBRUSH 处置，余下两条的根因在本目录
# 之外（libs/pigment 与 libs/brush），详见
# .superpowers/sdd/R-80/libs-image-report.md §KisPaintOpPresetTest。
# 截至本注释最后一次修订，该测试整体为 5 passed / 2 failed。
if(APPLE)
    set(KRITA_DEFAULTPAINTOPS_WHOLE_ARCHIVE "-Wl,-force_load,$<TARGET_FILE:kritadefaultpaintops>")
else()
    set(KRITA_DEFAULTPAINTOPS_WHOLE_ARCHIVE
        "-Wl,--whole-archive" "$<TARGET_FILE:kritadefaultpaintops>" "-Wl,--no-whole-archive")
endif()

# --- R-86：`pk_add_tests` 块的滤镜 provider 强制链入 -------------------------
# 与上面 KRITA_BLURFILTER_WHOLE_ARCHIVE 完全同一个机理（D-12 静态注册 ⇒
# 自注册静态对象所在的那个 .o 无人引用就被链接器整份丢掉 ⇒ 构造函数不运行 ⇒
# `KisFilterRegistry::instance()->value("<id>")` 返回 null ⇒ NDEBUG 下
# `Q_ASSERT` 空转 ⇒ 空指针解引用）。
#
# **为什么单独立一节**：上面的 bloc 只服务 `kis_add_tests`（QTest 栈）与几个
# 点名 target；`pk_add_tests` 块（下方那个块，46 个 target；**不写行号**——
# 本文件的注释改动会让它立刻失效）的公共
# `LINK_LIBRARIES` 里**一个 provider 插件都没有**，于是该块里所有真正去取滤镜的
# 用例都是「跑起来才炸」。R-86 实测的 5 条 SegFault 全部落在这一块（映射见下）。
#
# **不铺开**：R-86 有意**不**把这几个变量塞进 `pk_add_tests` 的公共
# `LINK_LIBRARIES`（那会让 46 个 target 都拉 4 个滤镜库，且掩盖「谁需要谁」），
# 而是逐 target 用 `target_link_options` 点名（形制同下方 kis_selection_test /
# KisPaintOpPresetTest 那几段）。
#
# **为什么不再走「链 `plugins/kritaplugin_registry` 聚合器」那条路**（R-86 判定
# 阶段的实测，写在这里免得下一个人重试）：把聚合器 + `KRITAPLUGIN_REGISTRY_LIBS`
# 全部 118 个插件归档 `-force_load` 到 `kis_filter_test` 的真实链接行上 ⇒
# **link exit=1，230 个未定义符号**，全部落在 impex/产品闭包
# （`_TIFF*`/`_DGif*`/`_heif_*`/`Exiv2::*`/`KisDocument::setErrorMessage`/
# `KisImportExportErrorCode::*` …）⇒ 聚合器进测试 = 把整个产品闭包拖进每一个
# 测试二进制，**链接根本不成立**。聚合器的消费者是产品侧（M5 合拢时的 app/pk
# 消费者，今天不存在），那件事不在本文件。
#
# provider ↔ 滤镜 id 的来源（逐个在源码里核过 `KisFilter::id()`；
# **行号一律给 `id()` 的声明行**，id 字面量在其下一行）：
#   "invert"           ← plugins/filters/example/example.h:23（字面量 :24）
#   "colorbalance"     ← plugins/filters/colorsfilters/kis_color_balance_filter.h:33（:34）
#   "resettransparent" ← plugins/filters/resettransparent/KisResetTransparentFilter.h:19（:20）
#   "blur"             ← plugins/filters/blur/kis_blur_filter.h:25（:26；变量已在上方）
# 这 4 个 id 全树唯一（各自只有这一个提供者）。
if(APPLE)
    set(KRITA_EXAMPLEFILTER_WHOLE_ARCHIVE "-Wl,-force_load,$<TARGET_FILE:kritaexample>")
else()
    set(KRITA_EXAMPLEFILTER_WHOLE_ARCHIVE
        "-Wl,--whole-archive" "$<TARGET_FILE:kritaexample>" "-Wl,--no-whole-archive")
endif()
if(APPLE)
    set(KRITA_COLORSFILTERS_WHOLE_ARCHIVE "-Wl,-force_load,$<TARGET_FILE:kritacolorsfilters>")
else()
    set(KRITA_COLORSFILTERS_WHOLE_ARCHIVE
        "-Wl,--whole-archive" "$<TARGET_FILE:kritacolorsfilters>" "-Wl,--no-whole-archive")
endif()
if(APPLE)
    set(KRITA_RESETTRANSPARENT_WHOLE_ARCHIVE "-Wl,-force_load,$<TARGET_FILE:kritaresettransparent>")
else()
    set(KRITA_RESETTRANSPARENT_WHOLE_ARCHIVE
        "-Wl,--whole-archive" "$<TARGET_FILE:kritaresettransparent>" "-Wl,--no-whole-archive")
endif()

kis_add_test(
    KisPathCoverageMaskTest.cpp
    LINK_LIBRARIES kritatestsdk
    )


kis_add_tests(
    kis_fast_math_test.cpp
    kis_effect_mask_test.cpp
    kis_iterator_test.cpp
    kis_painter_test.cpp
    kis_pixel_selection_test.cpp
    kis_convolution_painter_test.cpp
    kis_datamanager_test.cpp
    kis_fill_painter_test.cpp
    kis_filter_strategy_test.cpp
    kis_image_commands_test.cpp
    kis_image_test.cpp
    kis_iterators_ng_test.cpp
    kis_updater_context_test.cpp
    kis_simple_update_queue_test.cpp
    kis_stroke_test.cpp
    kis_simple_stroke_strategy_test.cpp
    kis_stroke_strategy_undo_command_based_test.cpp
    kis_strokes_queue_test.cpp
    kis_math_toolbox_test.cpp
    kis_name_server_test.cpp
    kis_node_commands_test.cpp
    kis_node_visitor_test.cpp
    kis_paint_information_test.cpp
    kis_paintop_test.cpp
    kis_selection_mask_test.cpp
    kis_shared_ptr_test.cpp
    kis_warp_transform_worker_test.cpp
    kis_types_test.cpp
    kis_vec_test.cpp
    kis_mask_keyframe_roundtrip_test.cpp
    kis_fixed_point_maths_test.cpp
    kis_filter_weights_buffer_test.cpp
    kis_filter_weights_applicator_test.cpp
    kis_fill_interval_test.cpp
    kis_fill_interval_map_test.cpp
    kis_psd_layer_style_test.cpp
    kis_layer_style_projection_plane_test.cpp
    kis_lod_capable_layer_offset_test.cpp
    kis_algebra_2d_test.cpp
    kis_lazy_brush_test.cpp
    KisMaskGeneratorBenchmark.cpp
    kis_layer_style_filter_environment_test.cpp
    kis_asl_parser_test.cpp
    KisPerStrokeRandomSourceTest.cpp
    kis_transform_worker_test.cpp
    kis_cs_conversion_test.cpp
    kis_projection_leaf_test.cpp
    kis_histogram_test.cpp
    kis_queues_progress_updater_test.cpp
    kis_walkers_test.cpp
    kis_cage_transform_worker_test.cpp
    KisRandomGenerator2DTest.cpp
    kis_time_span_test.cpp
    kis_asl_layer_style_serializer_test.cpp
    kis_selection_test.cpp
    kis_update_scheduler_test.cpp
    kis_colorize_mask_test.cpp
    kis_processings_test.cpp
    kis_paint_device_test.cpp
    kis_layer_styles_test.cpp
    kis_mesh_transform_worker_test.cpp
    KisKeyframeAnimationInterfaceSignalTest.cpp
    KisOverlayPaintDeviceWrapperTest.cpp
    KisPaintOpPresetTest.cpp
    KisGridInterpolationToolsTest.cpp
    LINK_LIBRARIES kritaimage kritatestsdk ${KRITA_LCMS_NO_AS_NEEDED} kritalcmsengine ${KRITA_AS_NEEDED} kritablurfilter ${KRITA_BLURFILTER_WHOLE_ARCHIVE}
    )

# R-65 Task 3：以下 46 个 target 走 pk 测试栈（pk_add_tests）。它们都在可达面
# （reachable-targets.txt，SIMPLE_TEST_MAIN 且不用 QBENCHMARK），基线里全红。
# LINK_LIBRARIES 与上面 kis_add_tests 块一致，只是不含 kritatestsdk（pk_add_test
# 内部链 kritatestsdk_pk）。
pk_add_tests(
    kis_base_node_test
    kis_node_test
    kis_node_facade_test
    kis_fixed_paint_device_test
    kis_layer_test
    kis_count_visitor_test
    kis_projection_test
    kis_properties_configuration_test
    kis_transaction_test
    kis_group_layer_test
    kis_paint_layer_test
    kis_adjustment_layer_test
    kis_annotation_test
    kis_crop_processing_visitor_test
    kis_processing_applicator_test
    kis_filter_configuration_test
    kis_filter_test
    kis_filter_processing_information_test
    kis_filter_registry_test
    kis_gradient_painter_test
    kis_image_config_test
    kis_image_signal_router_test
    kis_iterator_benchmark
    kis_mask_test
    kis_node_graph_listener_test
    kis_distance_information_test
    kis_pattern_test
    kis_bsplines_test
    kis_transparency_mask_test
    kis_mask_generator_test
    kis_cubic_curve_test
    kis_cubic_spline_test
    kis_node_query_path_test
    kis_scanline_fill_test
    kis_marker_painter_test
    kis_mask_similarity_test
    KisWatershedWorkerTest
    kis_dom_utils_test
    kis_onion_skin_compositor_test
    kis_image_animation_interface_test
    kis_keyframing_test
    kis_filter_mask_test
    TestAslStorage
    kis_async_merger_test
    KisFourPointInterpolatorTest
    KisSpatialContainerTest
    LINK_LIBRARIES kritaimage ${KRITA_LCMS_NO_AS_NEEDED} kritalcmsengine ${KRITA_AS_NEEDED}
    )

# R-80：pk 块的这三个 target 也吃真色彩管理。上面那一行里的 kritalcmsengine
# 在 APPLE 上是纯静态库引用、无符号引用即整份丢弃（KRITA_LCMS_NO_AS_NEEDED
# 在 APPLE 为空，且 --no-as-needed 对静态库本就无效），所以引擎的自注册静态
# 对象 LcmsEngineRegistration 从来不运行，色彩空间全部退回
# KoSimpleColorSpace（"unmanaged"）。TestAslStorage 读 ASL 里的颜色时会逐条打
# "Undefined operation in the ... color space"（本段就是消这条告警的：
# 与 kis_fill_painter_test / kis_selection_test 同一个机理、同一份处置）。
#
# ⚠ 订正（R-80，实测两次）：本段**不是** TestAslStorage 的解药，它只消上面那条
# unmanaged 告警。本段保留，理由就是那一句。
#
# ⚠⚠ 订正二（R-78，2026-09-15，实测）：上面那句「理由就是那一句」**低估了本段**——
# 本段不只是消告警，它是 `kis_scanline_fill_test` **10 条用例全过**的必要条件。
# 受控实验（同一棵树、同一份源码，只把下面那三行 foreach 注释掉，重建同一 target）：
#   本段在   → ./bin/kis_scanline_fill_test  exit=0 · Totals: 10 passed, 0 failed
#              · "KoSimpleColorSpace … unmanaged" 告警 0 条
#   本段不在 → exit=4 · Totals: 6 passed, 4 failed · 同一条告警 9 条
#              · 首错 kis_scanline_fill_test.cpp:58  QCOMPARE(c, expectedResult[i])
#                     kis_scanline_fill_test.cpp:277 QCOMPARE(other->exactBounds(), rc1)
# 机理与上面那段同源：没有 lcms 引擎时 `rgb8()` 退回 KoSimpleColorSpace，
# `KoColor(…, cs)` 与 `device->pixel(x,y,&c)` 的颜色转换变成「未定义操作」，于是
# **颜色断言与 `exactBounds()` 双双对不上**。M0（Linux/CI，lcms 动态装配）没有这一步，
# 所以 M0 是绿的，中间那段红纯粹是 macOS 上静态库的链接形态。
#
# ⇒ **不要因为「本段只是消告警」就把它删掉；也不要把 `kis_scanline_fill_test` 从这个
#    foreach 里摘出去**（除非它另有拿到 lcms 引擎的途径）——摘掉就是 4 条断言静默变红。
#    这个坑的代价已经付过两次：失败信息是 `<unprintable>`（`PK_COMPARE` 对
#    `PkColor`/`PkRect` 没有 toString，见 R-87），R-75 据此把它误判成「剥 Qt 后的行为
#    差异」并立了案，R-78 才查出它其实是链接形态问题。
#    口径订正：R-75 记的「6/10 Failed」按 QTest 的 Totals 行读应是「6 passed / 10」。
#    （第三个 target `kis_image_animation_interface_test` 对本段的依赖 R-78 **未实测**，
#      不要照上面那两行外推。）
#
# 而 TestAslStorage 的**真根因不在本文件能修的地方**，记录如下（R-80 实测，避免
# 下一个人重走）：
#   ① 该测试在 `pk_add_tests` 块（pk 栈）。pk 栈**没有** `FILES_DEFAULT_DATA_DIR`
#      （`sdk/tests/pk_test_support.cmake:230` 只设了 `FILES_DATA_DIR`），于是
#      `TestUtil::fetchDataFileLazy` 的第二条回退退化成 `testutil.h:72` 的默认值
#      `"."`，永远命不中 `sdk/tests/data/`。上游 v6.0.3 **没有**
#      `libs/image/tests/data/asl/`（GitHub API 实测 404），`.asl` 只在
#      `sdk/tests/data/asl/` —— 上游正是靠这条回退找到它们的。
#   ② R-80 曾把 5 个 `.asl` 复制进 `libs/image/tests/data/asl/` 让解析真正跑起来，
#      **结果是把一个更坏的状态引进来**：`libs/psdutils/asl/kis_asl_xml_parser.cpp`
#      的 descriptor 循环里两条 `continue`（:455 / :465）跳过了循环末尾的
#      `child = child.nextSibling()`（上游 v6.0.3 同一段同形，是上游潜伏 bug），
#      一旦那条数据真的被解析就**原地死循环** ⇒ 该测试从 `Failed 0.04 s` 变成
#      `Timeout 200 s`。**那是行为回归，所以 R-80 已把复制回退掉**，该测试恢复成
#      与 BASE 一致的快速失败。
#   ⇒ 真修法两处都在 R-80 锁外：① `sdk/tests/`（给 pk 栈补 `FILES_DEFAULT_DATA_DIR`）；
#      ② `libs/psdutils/asl/kis_asl_xml_parser.cpp`（**已被 R-42 / R-43 显式锁定**）。
#      逐条证据见 `.superpowers/sdd/R-80/libs-image-report.md` 与
#      `.superpowers/sdd/R-80/evidence/structural-gaps.md`。
foreach(_r80_target TestAslStorage kis_scanline_fill_test kis_image_animation_interface_test)
    target_link_options(${_r80_target} PRIVATE ${KRITA_LCMS_WHOLE_ARCHIVE})
endforeach()

# KisBezierGradientMesh.cpp is excluded from kritaimage, but
# kis_mesh_transform_worker_test still owns its diagnostic render coverage.
target_sources(kis_mesh_transform_worker_test PRIVATE ../KisBezierGradientMesh.cpp)

# kis_fill_painter_test 需要真实色彩管理：它的参考图是在 lcms 引擎注册的
# 色彩空间下生成的，缺引擎时会退回 KoSimpleColorSpace（"unmanaged"），
# 填充结果与参考图差 1 像素。D-12 之后插件是静态库，必须 --whole-archive
# 才能把它的自注册静态对象（LcmsEngineRegistration）拉进来；--no-as-needed
# 对静态库无效（静态库按符号引用取成员，无引用的成员照样被丢弃）。
target_link_options(kis_fill_painter_test PRIVATE ${KRITA_LCMS_WHOLE_ARCHIVE})

# R-80：灰度色彩空间的消费者，与上面 kis_fill_painter_test 同一个机理。
# kis_selection_test 的两条用例直接取 GRAYA/U8
# （kis_selection_test.cpp:81 与 :115），而 GRAYA 的 KoColorSpaceFactory
# 只由 lcms2engine 提供（plugins/color/lcms2engine/colorspaces/gray_u8/），
# 它的注册同样是静态全局对象 LcmsEngineRegistration 自注册，不 force_load
# 就整份丢掉。kis_add_tests 块的 LINK_LIBRARIES 里虽然写了 kritalcmsengine，
# 但 APPLE 上 KRITA_LCMS_NO_AS_NEEDED 为空、且 --no-as-needed 对静态库本就无效，
# 所以那一行在 APPLE 上不产生任何强制包含。
# 实测：修前 QVERIFY(csA) 在 :89 / :122 返回 FALSE；修后通过。
target_link_options(kis_selection_test PRIVATE ${KRITA_LCMS_WHOLE_ARCHIVE})

# R-80：KisPaintOpPresetTest 需要 paintbrush/duplicate 两个 paintop 工厂。
# 上面的 kis_add_tests 块把 kritadefaultpaintops 的注册 .cc 编进了链，但
# 静态库按符号引用取成员——没人引用就整份丢弃，所以注册不运行。故与
# kis_selection_test / kis_fill_painter_test 同形：先 target_link_libraries
# 把库（及其传递依赖 kritadefaultpaintops_static 里的 KisBrushOp 等实现）
# 拉进链接行，再用 -force_load 强制包含注册对象所在的那个成员。
target_link_libraries(KisPaintOpPresetTest kritadefaultpaintops)
target_link_options(KisPaintOpPresetTest PRIVATE ${KRITA_DEFAULTPAINTOPS_WHOLE_ARCHIVE})

# R-80：KisPaintOpPresetTest 还缺 brush/pattern 之外的 brush 加载器注册。
# 该测试 include 的是 <testimage.h>（只定义 TESTIMAGE），而 sdk/tests/kistest.h
# 把 4 个 brush 加载器（KisGbrBrush/KisImagePipeBrush/KisSvgBrush/KisPngBrush）
# 关在 `#if defined(TESTBRUSH) || defined(TESTUI)` 里。上游同一份 kistest.h
# 守卫逐字相同（v6.0.3 已核对），所以这是上游测试 SDK 的既有形态，不是移植回归。
# 后果：test-embedded-resources-5.0.kpp 内嵌两个资源
#   type="brushes" filename="test_brush.png"
#   type="patterns" filename="test_pattern.png"
# 其中 patterns 加载器在第一个（含 TESTIMAGE）守卫块里，brushes 那个不在 ——
# 于是 KisPaintOpPresetTest.cpp:325 的
#   KisResourceLoaderRegistry::instance()->loader(signature.type, mime)
# 对 type="brushes" 恒返回 nullptr（实测 lldb：signature.type 长 7 = "brushes"，
# filename 长 14 = "test_brush.png"）。
# 修法：只给这一个 target 追加 TESTBRUSH 编译定义，让 kistest.h 把 brush 加载器
# 一并注册。守卫是 ||，TESTIMAGE 仍然生效，属纯增量，不改任何参考图/容差/断言。
# 实测（本 worktree，各 ×3 稳定）：不加本行时 testSaveLoadRoundTrip 在
# KisPaintOpPresetTest.cpp:325 的 QVERIFY(loader) 即 FALSE；加本行后该断言通过、
# 失败点后移到 :332（另一条独立缺口，根因在 libs/brush，见报告）。
# 用例计数两态相同（5 passed / 2 failed）——本行修的是 :325 这个真实缺口，
# 不是「把红的改成绿的」。
target_compile_definitions(KisPaintOpPresetTest PRIVATE TESTBRUSH)

# R-86：`pk_add_tests` 块里 5 个取滤镜的 target —— 逐 target 窄链 provider。
# 变量定义与机理说明见文件上方「R-86：`pk_add_tests` 块的滤镜 provider 强制链入」。
#
# 每一条的「哪个用例、取哪个 id、修前怎么炸」都跑出来核过（不是按测试名猜的）：
# 修前 5 个 target 全部 `exit=139`（SegFault），`lldb` 首错落在
# `f->defaultConfiguration(...)`（`-DNDEBUG` 下 `Q_ASSERT(f)` 空转）。
# 修后 5 个 target 全部 `exit=0`，Totals 依次 8/5/12/6/8 passed、0 failed。
# 实测口径：`HOME=<新目录>` · `QT_QPA_PLATFORM=offscreen` ·
# `DYLD_LIBRARY_PATH=<build>/bin`；与 `ctest -R '^<name>$'` 结果一致。
#
# ① kis_filter_test —— 取 `value("invert")`（:65）与 `value("blur")`（:218）；
#    **首错落在其后的解引用行** `f->defaultConfiguration(...)`，即
#    `kis_filter_test.cpp:68:40` 与 `:220:40`。只补 invert 时首错从 :68 后移到 :220
#    （这条「后移」本身就是「两个 provider 都缺」的证据）。
target_link_options(kis_filter_test PRIVATE
    ${KRITA_EXAMPLEFILTER_WHOLE_ARCHIVE} ${KRITA_BLURFILTER_WHOLE_ARCHIVE})

# ② kis_filter_mask_test —— `kis_filter_mask_test.cpp:111` `value("resettransparent")`
#    + `value("invert")`。
target_link_options(kis_filter_mask_test PRIVATE
    ${KRITA_EXAMPLEFILTER_WHOLE_ARCHIVE} ${KRITA_RESETTRANSPARENT_WHOLE_ARCHIVE})

# ③ kis_async_merger_test —— `kis_async_merger_test.cpp:477` `value("colorbalance")`
#    + `value("blur")` + `value("invert")`。
target_link_options(kis_async_merger_test PRIVATE
    ${KRITA_EXAMPLEFILTER_WHOLE_ARCHIVE} ${KRITA_BLURFILTER_WHOLE_ARCHIVE}
    ${KRITA_COLORSFILTERS_WHOLE_ARCHIVE})

# ④ kis_adjustment_layer_test —— 只取 `value("invert")`。
target_link_options(kis_adjustment_layer_test PRIVATE ${KRITA_EXAMPLEFILTER_WHOLE_ARCHIVE})

# ⑤ kis_layer_test —— 只取 `value("blur")`。
target_link_options(kis_layer_test PRIVATE ${KRITA_BLURFILTER_WHOLE_ARCHIVE})
#
# **同一个坑还在等着的地方（R-86 判定阶段实测，别以为只有这 5 条）**：
# `pk_add_tests` 块里另有 `kis_walkers_test`（blur + invert）、`kis_clone_layer_test`（blur）、
# `kis_image_test`（blur）、`kis_paint_layer_test`（blur 族）；块外还有
# `plugins/impex/libkra/tests/util.h:119`（pixelize ← `kritapixelizefilter`）与
# `plugins/filters/unsharp/tests`（unsharp ← `kritaunsharpfilter`）。
# 它们**今天全部编不出来（ctest 报 `Not Run`）**，所以给它们接线**一条都验证不了** ——
# 按流水线「不收零证据改动」的规矩，本任务只收能验证的这 5 条，其余登记在
# `.superpowers/sdd/R-86/impact-map.md` §4，交后续任务在它们编得过之后照本段形制补。

kis_add_test(
    kis_clone_layer_test.cpp
    LINK_LIBRARIES kritaimage kritatestsdk kritaglobal kritatransformmaskstubs
    )

kis_add_test(
    kis_transform_mask_test.cpp
    LINK_LIBRARIES kritaimage kritatestsdk kritaglobal kritatransformmaskstubs
    )


include_directories(${QUAZIP_INCLUDE_DIRS})

kis_add_test(
    kis_liquify_transform_worker_test.cpp
    LINK_LIBRARIES kritaimage kritatestsdk ${QUAZIP_LIBRARIES}
    )

krita_add_broken_unit_tests(
    kis_perspective_transform_worker_test.cpp
    LINK_LIBRARIES kritaimage kritatestsdk
    )

if(APPLE)
    set_property(TARGET kis_image_animation_interface_test PROPERTY MACOSX_BUNDLE ON)
    set_property(TARGET kis_cage_transform_worker_test PROPERTY MACOSX_BUNDLE ON)

    macos_test_fixrpath(KisRandomGenerator2DDemo)
endif()

kis_add_test(
    KisSafeDocumentLoaderTest.cpp
    TEST_NAME KisSafeDocumentLoaderTest
    LINK_LIBRARIES kritaimage kritatestsdk
)

kis_add_test(
    kis_file_layer_test.cpp
    LINK_LIBRARIES kritaimage kritatestsdk kritaglobal kritatransformmaskstubs
)

kis_add_test(
    KisColorSamplingUtilsTest.cpp
    LINK_LIBRARIES kritaimage kritatestsdk
)

krita_add_broken_unit_test(
    filter_stroke_test.cpp
    ../../../sdk/tests/stroke_testing_utils.cpp
    TEST_NAME FilterStrokeTest
    NAME_PREFIX libs-image-
    LINK_LIBRARIES kritaimage kritatestsdk
)

# R-72 Task 1 降级 driver（R线-spec「依赖墙挡住真实测试类时：允许降级成复刻调用点
# 形状的 driver」）：逐行复刻 `KisImageTest::testAssignImageProfile`
# （`kis_image_test.cpp:216–260`）里 p709SRGBProfile()/p2020G10Profile() 的调用点形状
# （`rgb8()` → `new KisImage(0,1000,1000,rgb8,"stest")` → 剖面 `operator==` 比较）。
# 真实测试类本树编不过（四类源码层阻塞，见该 .cpp 头注释与 R-72 plan §1 F3），故用
# plain main() driver 落常驻证据。零 Qt：只链 kritaimage + kritalcmsengine，
# 除 kritaimage/kritapigment 传递的包含目录外，只需 lcms 引擎头（registerLcmsEngine）。
# 这堵墙什么时候拆：S 线完成 `libs/image` 源码迁移 + R-65 收口测试侧 target。
kis_add_test(
    PkImageProfileNoResourceDirsDriver.cpp
    LINK_LIBRARIES kritaimage kritalcmsengine
)
target_include_directories(PkImageProfileNoResourceDirsDriver PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/color/lcms2engine
)
