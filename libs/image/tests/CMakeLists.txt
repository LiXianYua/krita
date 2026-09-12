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

kis_add_test(
    KisPathCoverageMaskTest.cpp
    LINK_LIBRARIES kritatestsdk
    )


kis_add_tests(
    kis_base_node_test.cpp
    kis_fast_math_test.cpp
    kis_node_test.cpp
    kis_node_facade_test.cpp
    kis_fixed_paint_device_test.cpp
    kis_layer_test.cpp
    kis_effect_mask_test.cpp
    kis_iterator_test.cpp
    kis_painter_test.cpp
    kis_count_visitor_test.cpp
    kis_projection_test.cpp
    kis_properties_configuration_test.cpp
    kis_transaction_test.cpp
    kis_pixel_selection_test.cpp
    kis_group_layer_test.cpp
    kis_paint_layer_test.cpp
    kis_adjustment_layer_test.cpp
    kis_annotation_test.cpp
    kis_convolution_painter_test.cpp
    kis_crop_processing_visitor_test.cpp
    kis_processing_applicator_test.cpp
    kis_datamanager_test.cpp
    kis_fill_painter_test.cpp
    kis_filter_configuration_test.cpp
    kis_filter_test.cpp
    kis_filter_processing_information_test.cpp
    kis_filter_registry_test.cpp
    kis_filter_strategy_test.cpp
    kis_gradient_painter_test.cpp
    kis_image_commands_test.cpp
    kis_image_config_test.cpp
    kis_image_test.cpp
    kis_image_signal_router_test.cpp
    kis_iterators_ng_test.cpp
    kis_iterator_benchmark.cpp
    kis_updater_context_test.cpp
    kis_simple_update_queue_test.cpp
    kis_stroke_test.cpp
    kis_simple_stroke_strategy_test.cpp
    kis_stroke_strategy_undo_command_based_test.cpp
    kis_strokes_queue_test.cpp
    kis_mask_test.cpp
    kis_math_toolbox_test.cpp
    kis_name_server_test.cpp
    kis_node_commands_test.cpp
    kis_node_graph_listener_test.cpp
    kis_node_visitor_test.cpp
    kis_paint_information_test.cpp
    kis_distance_information_test.cpp
    kis_paintop_test.cpp
    kis_pattern_test.cpp
    kis_selection_mask_test.cpp
    kis_shared_ptr_test.cpp
    kis_bsplines_test.cpp
    kis_warp_transform_worker_test.cpp
    kis_transparency_mask_test.cpp
    kis_types_test.cpp
    kis_vec_test.cpp
    kis_mask_generator_test.cpp
    kis_mask_keyframe_roundtrip_test.cpp
    kis_cubic_curve_test.cpp
    kis_cubic_spline_test.cpp
    kis_fixed_point_maths_test.cpp
    kis_node_query_path_test.cpp
    kis_filter_weights_buffer_test.cpp
    kis_filter_weights_applicator_test.cpp
    kis_fill_interval_test.cpp
    kis_fill_interval_map_test.cpp
    kis_scanline_fill_test.cpp
    kis_psd_layer_style_test.cpp
    kis_layer_style_projection_plane_test.cpp
    kis_lod_capable_layer_offset_test.cpp
    kis_algebra_2d_test.cpp
    kis_marker_painter_test.cpp
    kis_lazy_brush_test.cpp
    kis_mask_similarity_test.cpp
    KisMaskGeneratorBenchmark.cpp
    kis_layer_style_filter_environment_test.cpp
    kis_asl_parser_test.cpp
    KisPerStrokeRandomSourceTest.cpp
    KisWatershedWorkerTest.cpp
    kis_dom_utils_test.cpp
    kis_transform_worker_test.cpp
    kis_cs_conversion_test.cpp
    kis_projection_leaf_test.cpp
    kis_histogram_test.cpp
    kis_onion_skin_compositor_test.cpp
    kis_queues_progress_updater_test.cpp
    kis_image_animation_interface_test.cpp
    kis_walkers_test.cpp
    kis_cage_transform_worker_test.cpp
    KisRandomGenerator2DTest.cpp
    kis_keyframing_test.cpp
    kis_time_span_test.cpp
    kis_filter_mask_test.cpp
    kis_asl_layer_style_serializer_test.cpp
    TestAslStorage.cpp
    kis_async_merger_test.cpp
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
    KisFourPointInterpolatorTest.cpp
    KisSpatialContainerTest.cpp
    KisGridInterpolationToolsTest.cpp
    LINK_LIBRARIES kritaimage kritatestsdk ${KRITA_LCMS_NO_AS_NEEDED} kritalcmsengine ${KRITA_AS_NEEDED}
    )

# KisBezierGradientMesh.cpp is excluded from kritaimage, but
# kis_mesh_transform_worker_test still owns its diagnostic render coverage.
target_sources(kis_mesh_transform_worker_test PRIVATE ../KisBezierGradientMesh.cpp)

# kis_fill_painter_test 需要真实色彩管理：它的参考图是在 lcms 引擎注册的
# 色彩空间下生成的，缺引擎时会退回 KoSimpleColorSpace（"unmanaged"），
# 填充结果与参考图差 1 像素。D-12 之后插件是静态库，必须 --whole-archive
# 才能把它的自注册静态对象（LcmsEngineRegistration）拉进来；--no-as-needed
# 对静态库无效（静态库按符号引用取成员，无引用的成员照样被丢弃）。
target_link_options(kis_fill_painter_test PRIVATE ${KRITA_LCMS_WHOLE_ARCHIVE})

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
