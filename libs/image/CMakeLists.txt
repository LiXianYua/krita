add_subdirectory( tests )

add_subdirectory( tiles3 )

# 判据⑧：与薄壳 SHELL_INCLUDE_DIRS 的 libs/image 子目录集一致（补上
# 拆分时漏掉的 tiles3/tiles3-swap/lazybrush/3rdparty-lock_free_map 与 libs/image 根），
# 再补全部 pk 模块源目录。compat 垫片目录不加进主树——那是薄壳专属的
# Q→Pk 宏映射机制，主树 GAP 文件仍依赖真实 Qt 头。
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles3
    ${CMAKE_CURRENT_SOURCE_DIR}/tiles3/swap
    ${CMAKE_CURRENT_SOURCE_DIR}/brushengine
    ${CMAKE_CURRENT_SOURCE_DIR}/commands
    ${CMAKE_CURRENT_SOURCE_DIR}/commands_new
    ${CMAKE_CURRENT_SOURCE_DIR}/filter
    ${CMAKE_CURRENT_SOURCE_DIR}/floodfill
    ${CMAKE_CURRENT_SOURCE_DIR}/generator
    ${CMAKE_CURRENT_SOURCE_DIR}/layerstyles
    ${CMAKE_CURRENT_SOURCE_DIR}/lazybrush
    ${CMAKE_CURRENT_SOURCE_DIR}/processing
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/lock_free_map
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/port
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/global
    ${CMAKE_SOURCE_DIR}/pk/log
    ${CMAKE_SOURCE_DIR}/pk/concurrent
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/time
    ${CMAKE_SOURCE_DIR}/pk/config
    ${CMAKE_SOURCE_DIR}/pk/flags
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/signal
    ${CMAKE_SOURCE_DIR}/pk/sql
    ${CMAKE_SOURCE_DIR}/pk/xml
    ${CMAKE_SOURCE_DIR}/pk/namespace
)

if(FFTW3_FOUND)
  include_directories(${FFTW3_INCLUDE_DIR})
endif()

ko_compile_for_all_implementations_no_scalar(__per_arch_circle_mask_generator_objs kis_brush_mask_applicator_factories.cpp)
ko_compile_for_all_implementations_no_scalar(_per_arch_processor_objs kis_brush_mask_processor_factories.cpp)

message("Following objects are generated from the per-arch lib")
foreach(_obj IN LISTS __per_arch_circle_mask_generator_objs _per_arch_processor_objs)
  message("    * ${_obj}")
endforeach()

# ---------------------------------------------------------------------------
# S-06 阶段① 结构拆分（2026-08-19）：把平铺 SHARED 拆成「多个中间 OBJECT 库
# + 一个汇总 SHARED」。代码语义零变化 —— 最终 SHARED 链接的仍是同一批源文件的
# 同一批目标文件。为阶段②自底向上分层剥离制造停靠点。
#
# 依赖闭包（阶段②先后剥的依据）：
#   每个 OBJECT 库以 PRIVATE 链接 kritaimage 的完整编译期依赖集（见下
#   kritaimage_OBJECT_DEPS），从而获得与拆分前 SHARED 完全一致的
#   include/宏环境；PRIVATE 保证这些依赖不泄漏到最终 SHARED 的链接接口
#   （最终 SHARED 保留自己的 target_link_libraries 不变）。
#   -Dkritaimage_EXPORTS 由 SHARED 自动注入、OBJECT 库不会，这里显式补上，
#   使每个源文件的编译命令行与拆分前逐项一致（含 kritaimage_export.h 走
#   「building this library」分支）。
# ---------------------------------------------------------------------------
set(kritaimage_OBJECT_DEPS
    kritaversion
    kritaglobal
    kritapsdutils
    kritapigment
    kritacommand
    kritametadata
    kritaresources
    Eigen3::Eigen
    Boost::boost
    # 判据⑨：pk 库接入主构建（add_subdirectory(pk) 已在顶层无条件开启）。
    # 与薄壳的实测编译闭包同集；KisImageConfig 额外需要
    # pkconfig 和 pkconcurrent。
    pkstring
    pkcontainer
    pkzip
    pkxml
    pkconfig
    pkconcurrent
    pksignal
)
if(HAVE_CXX_ATOMICS_WITH_LIB OR HAVE_CXX_ATOMICS64_WITH_LIB)
    list(APPEND kritaimage_OBJECT_DEPS atomic)
endif()
if(OpenEXR_FOUND)
    list(APPEND kritaimage_OBJECT_DEPS ${LINK_OPENEXR_LIB})
endif()
list(APPEND kritaimage_OBJECT_DEPS kritamultiarch)
if(NOT GSL_FOUND)
    message(WARNING "KRITA WARNING! No GNU Scientific Library was found! Krita's Shaped Gradients might be non-normalized! Please install GSL library.")
else()
    list(APPEND kritaimage_OBJECT_DEPS GSL::gsl)
endif()

# helper：定义一个 OBJECT 中间库 + 挂上共享的编译期依赖环境
function(kis_define_image_object_lib libname)
    cmake_parse_arguments(OBJ "" "" "SOURCES" ${ARGN})
    kis_add_library(${libname} OBJECT ${OBJ_SOURCES})
    target_link_libraries(${libname} PRIVATE ${kritaimage_OBJECT_DEPS})
    target_compile_definitions(${libname} PRIVATE kritaimage_EXPORTS)
endfunction()

# —— tiles3（含 swap 私有后端；底层像素瓦片数据管理层）——

kis_define_image_object_lib(kritaimage_tiles3_objects SOURCES
    tiles3/kis_tile.cc
    tiles3/kis_tile_data.cc
    tiles3/kis_tile_data_store.cc
    tiles3/kis_tile_data_pooler.cc
    tiles3/kis_tiled_data_manager.cc
    tiles3/KisTiledExtentManager.cpp
    tiles3/kis_memento_manager.cc
    tiles3/kis_hline_iterator.cpp
    tiles3/kis_vline_iterator.cpp
    tiles3/kis_random_accessor.cc
    tiles3/swap/kis_abstract_compression.cpp
    tiles3/swap/kis_lzf_compression.cpp
    tiles3/swap/kis_abstract_tile_compressor.cpp
    tiles3/swap/kis_legacy_tile_compressor.cpp
    tiles3/swap/kis_tile_compressor_2.cpp
    tiles3/swap/kis_chunk_allocator.cpp
    tiles3/swap/kis_memory_window.cpp
    tiles3/swap/kis_swapped_data_store.cpp
    tiles3/swap/kis_tile_data_swapper.cpp
)

# —— brushengine（笔刷引擎）——

kis_define_image_object_lib(kritaimage_brushengine_objects SOURCES
    brushengine/kis_paint_information.cc
    brushengine/KisPerStrokeRandomSource.cpp
    brushengine/kis_stroke_random_source.cpp
    brushengine/kis_paintop.cc
    brushengine/kis_paintop_factory.cpp
    brushengine/kis_paintop_preset.cpp
    brushengine/kis_paintop_registry.cc
    brushengine/kis_paintop_settings.cpp
    brushengine/KisPaintOpPresetUpdateProxy.cpp
    brushengine/kis_paintop_utils.cpp
    brushengine/kis_no_size_paintop_settings.cpp
    brushengine/kis_locked_properties.cc
    brushengine/kis_locked_properties_proxy.cpp
    brushengine/kis_locked_properties_server.cpp
    brushengine/kis_uniform_paintop_property.cpp
    brushengine/kis_combo_based_paintop_property.cpp
    brushengine/kis_slider_based_paintop_property.cpp
    brushengine/kis_standard_uniform_properties_factory.cpp
    brushengine/KisStrokeSpeedMeasurer.cpp
    brushengine/KisPaintopSettingsIds.cpp
    brushengine/KisOptimizedBrushOutline.cpp
)

# —— commands + commands_new（命令/撤销栈）——

kis_define_image_object_lib(kritaimage_commands_objects SOURCES
    commands/kis_deselect_global_selection_command.cpp
    commands/KisDeselectActiveSelectionCommand.cpp
    commands/kis_image_change_layers_command.cpp
    commands/kis_image_change_visibility_command.cpp
    commands/kis_image_command.cpp
    commands/kis_image_layer_add_command.cpp
    commands/kis_image_layer_move_command.cpp
    commands/kis_image_layer_remove_command.cpp
    commands/kis_image_layer_remove_command_impl.cpp
    commands/kis_node_command.cpp
    commands/kis_node_compositeop_command.cpp
    commands/kis_node_opacity_command.cpp
    commands/kis_node_property_list_command.cpp
    commands/kis_reselect_global_selection_command.cpp
    commands/KisReselectActiveSelectionCommand.cpp
    commands/kis_set_global_selection_command.cpp
    commands/KisNodeRenameCommand.cpp
    commands/KisChangeDeselectedMaskCommand.cpp
    commands/KisNotifySelectionChangedCommand.cpp
    commands/KisBatchUpdateLayerModificationCommand.cpp
    commands_new/kis_saved_commands.cpp
    commands_new/kis_processing_command.cpp
    commands_new/kis_image_resize_command.cpp
    commands_new/kis_image_set_resolution_command.cpp
    commands_new/kis_node_move_command2.cpp
    commands_new/kis_set_layer_style_command.cpp
    commands_new/kis_selection_move_command2.cpp
    commands_new/kis_update_command.cpp
    commands_new/kis_switch_current_time_command.cpp
    commands_new/kis_change_projection_color_command.cpp
    commands_new/kis_activate_selection_mask_command.cpp
    commands_new/kis_transaction_based_command.cpp
    commands_new/KisHoldUIUpdatesCommand.cpp
    commands_new/KisImageAnimSettingCommand.cpp
    commands_new/KisChangeChannelFlagsCommand.cpp
    commands_new/KisChangeChannelLockFlagsCommand.cpp
    commands_new/KisMergeLabeledLayersCommand.cpp
    commands_new/KisAsynchronouslyMergeableCommandInterface.cpp
    commands_new/KisSimpleModifyTransformMaskCommand.cpp
    commands_new/KisUpdateCommandEx.cpp
    commands_new/KisDisableDirtyRequestsCommand.cpp
    commands_new/KisResetGroupLayerCacheCommand.cpp
    commands_new/KisLazyCreateTransformMaskKeyframesCommand.cpp
    commands_new/KisChangeCloneLayersCommand.cpp
    commands_new/KisLayerCollapseCommand.cpp
)

# —— processing（处理访问者）——

kis_define_image_object_lib(kritaimage_processing_objects SOURCES
    processing/kis_do_nothing_processing_visitor.cpp
    processing/kis_simple_processing_visitor.cpp
    processing/kis_convert_color_space_processing_visitor.cpp
    processing/kis_assign_profile_processing_visitor.cpp
    processing/kis_crop_processing_visitor.cpp
    processing/kis_crop_selections_processing_visitor.cpp
    processing/kis_transform_processing_visitor.cpp
    processing/kis_mirror_processing_visitor.cpp
    processing/KisSelectionBasedProcessingHelper.cpp
)

# —— filter（滤镜）——

kis_define_image_object_lib(kritaimage_filter_objects SOURCES
    filter/kis_filter.cc
    filter/kis_filter_category_ids.cpp
    filter/kis_filter_configuration.cc
    filter/kis_color_transformation_configuration.cc
    filter/kis_filter_registry.cc
    filter/kis_color_transformation_filter.cc
)

# —— generator（生成器）——

kis_define_image_object_lib(kritaimage_generator_objects SOURCES
    generator/kis_generator.cpp
    generator/kis_generator_layer.cpp
    generator/kis_generator_registry.cpp
    generator/kis_generator_stroke_strategy.cpp
)

# —— floodfill（洪水填充）——

kis_define_image_object_lib(kritaimage_floodfill_objects SOURCES
    floodfill/kis_fill_interval_map.cpp
    floodfill/kis_scanline_fill.cpp
    floodfill/kis_gap_map.cpp
)

# —— lazybrush（懒笔刷/分割上色）——

kis_define_image_object_lib(kritaimage_lazybrush_objects SOURCES
    lazybrush/kis_min_cut_worker.cpp
    lazybrush/kis_lazy_fill_tools.cpp
    lazybrush/kis_multiway_cut.cpp
    lazybrush/KisWatershedWorker.cpp
    lazybrush/kis_colorize_mask.cpp
    lazybrush/kis_colorize_stroke_strategy.cpp
)

# —— layerstyles（图层样式）——

kis_define_image_object_lib(kritaimage_layerstyles_objects SOURCES
    layerstyles/kis_multiple_projection.cpp
    layerstyles/kis_layer_style_filter.cpp
    layerstyles/kis_layer_style_filter_environment.cpp
    layerstyles/kis_layer_style_filter_projection_plane.cpp
    layerstyles/kis_layer_style_projection_plane.cpp
    layerstyles/KisStrokeLayerStyleFilterProjectionPlane.cpp
    layerstyles/kis_ls_drop_shadow_filter.cpp
    layerstyles/kis_ls_satin_filter.cpp
    layerstyles/kis_ls_stroke_filter.cpp
    layerstyles/kis_ls_bevel_emboss_filter.cpp
    layerstyles/kis_ls_overlay_filter.cpp
    layerstyles/kis_ls_utils.cpp
    layerstyles/gimp_bump_map.cpp
    layerstyles/KisLayerStyleKnockoutBlower.cpp
)

# —— bsplines（B 样条）——

kis_define_image_object_lib(kritaimage_bsplines_objects SOURCES
    bsplines/kis_bspline_1d.cpp
    bsplines/kis_bspline_2d.cpp
    bsplines/kis_nu_bspline_2d.cpp
)

# —— 顶层核心（KisImage/KisNode/KisPaintDevice/KisPainter 等）——

kis_define_image_object_lib(kritaimage_core_objects SOURCES
    # [GAP 排除] 13 个核心 GAP 文件（Task 8 批次A/C2 登记，判据⑨ 严格补）：仍依赖
    # 真实 Qt 头（QImage/QPainter/QFile/QTimer/KConfig 等）。与薄壳 SHELL_SOURCES
    # 的 [GAP 排除] 一致，从本 OBJECT 库源清单移除：
    #   kis_asl_layer_style_serializer.cpp  kis_psd_layer_style.cpp
    #   KisBezierGradientMesh.cpp           KisBezierTransformMesh.cpp
    #   kis_cage_transform_worker.cpp       kis_liquify_transform_worker.cpp
    #   kis_perspectivetransform_worker.cpp kis_warptransform_worker.cc
    #   kis_file_layer.cpp                  kis_safe_document_loader.cpp
    #   kis_paint_device_debug_utils.cpp
    #   KisSpatialContainer.cpp             kis_update_time_monitor.cpp
    # 移除后这些文件的符号在 kritaimage.so 中为未定义——链接允许，除非其他 OBJECT
    # 文件强引用（届时逐条登记）。GAP 配套头（kis_grid_interpolation_tools.h 等）
    # 不在源清单，无需移除。
    KisColorSamplerConfig.cpp
    kis_image_config.cpp
    KisColorSamplingUtils.cpp
    kis_distance_information.cpp
    kis_painter.cc
    kis_resources_snapshot.cpp
    kis_filter_stroke_strategy.cpp
    kis_painter_blt_multi_fixed.cpp
    kis_marker_painter.cpp
    KisOverlayPaintDeviceWrapper.cpp
    kis_progress_updater.cpp
    KisAnimatedOpacityProperty.cpp
    KisFrameChangeUpdateRecipe.cpp
    KisAnimAutoKey.cpp
    KisDelayedUpdateNodeInterface.cpp
    KisCroppedOriginalLayerInterface.cpp
    KisDecoratedNodeInterface.cpp
    kis_adjustment_layer.cc
    kis_selection_based_layer.cpp
    kis_node_filter_interface.cpp
    kis_base_accessor.cpp
    kis_base_node.cpp
    kis_base_processor.cpp
    kis_bookmarked_configuration_manager.cc
    KisBusyWaitBroker.cpp
    KisBatchNodeUpdate.cpp
    KisSafeBlockingQueueConnectionProxy.cpp
    kis_node_uuid_info.cpp
    kis_clone_layer.cpp
    kis_convolution_kernel.cc
    kis_convolution_painter.cc
    kis_gaussian_kernel.cpp
    kis_edge_detection_kernel.cpp
    KisWaveletKernel.cpp
    KisWaveletKernel.h
    kis_cubic_curve.cpp
    KisLevelsCurve.cpp
    KisAutoLevels.cpp
    kis_default_bounds.cpp
    kis_default_bounds_node_wrapper.cpp
    kis_default_bounds_base.cpp
    kis_effect_mask.cc
    kis_fast_math.cpp
    kis_fill_painter.cc
    kis_filter_mask.cpp
    kis_filter_strategy.cc
    kis_transform_mask.cpp
    kis_transform_mask_params_interface.cpp
    kis_recalculate_transform_mask_job.cpp
    kis_transform_mask_params_factory_registry.cpp
    KisTransformMaskTestingInterface.cpp
    kis_safe_transform.cpp
    kis_gradient_painter.cc
    kis_gradient_shape_strategy.cpp
    kis_cached_gradient_shape_strategy.cpp
    kis_polygonal_gradient_shape_strategy.cpp
    kis_iterator_ng.cpp
    kis_base_rects_walker.cpp
    kis_async_merger.cpp
    kis_merge_walker.cc
    kis_updater_context.cpp
    kis_update_job_item.cpp
    kis_stroke_strategy_undo_command_based.cpp
    kis_simple_stroke_strategy.cpp
    KisRunnableBasedStrokeStrategy.cpp
    KisRunnableStrokeJobDataBase.cpp
    KisRunnableStrokeJobData.cpp
    KisRunnableStrokeJobsInterface.cpp
    KisFakeRunnableStrokeJobsExecutor.cpp
    kis_stroke_job_strategy.cpp
    kis_stroke_strategy.cpp
    kis_stroke.cpp
    kis_strokes_queue.cpp
    KisStrokesQueueMutatedJobInterface.cpp
    kis_simple_update_queue.cpp
    kis_update_scheduler.cpp
    kis_queues_progress_updater.cpp
    kis_composite_progress_proxy.cpp
    kis_sync_lod_cache_stroke_strategy.cpp
    kis_lod_capable_layer_offset.cpp
    KisImageConfigNotifier.cpp
    kis_group_layer.cc
    kis_external_layer_iface.cc
    kis_count_visitor.cpp
    kis_histogram.cc
    kis_image_interfaces.cpp
    kis_image_animation_interface.cpp
    kis_time_span.cpp
    kis_node_graph_listener.cpp
    kis_image.cc
    kis_image_signal_router.cpp
    KisImageSignals.cpp
    KisImageResolutionProxy.cpp
    kis_projection_updates_filter.cpp
    kis_suspend_projection_updates_stroke_strategy.cpp
    kis_regenerate_frame_stroke_strategy.cpp
    kis_switch_time_stroke_strategy.cpp
    kis_crop_saved_extra_data.cpp
    kis_timed_signal_threshold.cpp
    kis_layer.cc
    kis_indirect_painting_support.cpp
    kis_abstract_projection_plane.cpp
    kis_layer_projection_plane.cpp
    kis_layer_utils.cpp
    kis_mask_projection_plane.cpp
    kis_projection_leaf.cpp
    KisSafeNodeProjectionStore.cpp
    kis_mask.cc
    kis_base_mask_generator.cpp
    kis_rect_mask_generator.cpp
    kis_circle_mask_generator.cpp
    kis_gauss_circle_mask_generator.cpp
    kis_gauss_rect_mask_generator.cpp
    kis_brush_mask_applicator_factories_Scalar.cpp
    kis_curve_circle_mask_generator.cpp
    kis_curve_rect_mask_generator.cpp
    kis_math_toolbox.cpp
    kis_memory_statistics_server.cpp
    kis_name_server.cpp
    kis_node.cpp
    kis_node_facade.cpp
    kis_node_progress_proxy.cpp
    kis_busy_progress_indicator.cpp
    kis_node_visitor.cpp
    kis_paint_device.cc
    kis_fixed_paint_device.cpp
    KisOptimizedByteArray.cpp
    kis_paint_layer.cc
    kis_perspective_math.cpp
    kis_pixel_selection.cpp
    kis_processing_information.cpp
    kis_properties_configuration.cc
    kis_random_accessor_ng.cpp
    KisRandomGenerator2D.cpp
    kis_random_sub_accessor.cpp
    kis_wrapped_random_accessor.cpp
    kis_selection.cc
    KisSelectionUtils.cpp
    KisSelectionUpdateCompressor.cpp
    kis_selection_mask.cpp
    kis_update_outline_job.cpp
    kis_update_selection_job.cpp
    kis_serializable_configuration.cc
    kis_transaction_data.cpp
    KisTransactionWrapperFactory.cpp
    KisInterstrokeDataTransactionWrapperFactory.cpp
    KisInterstrokeData.cpp
    KisInterstrokeDataFactory.cpp
    kis_transform_worker.cc
    kis_green_coordinates_math.cpp
    kis_transparency_mask.cc
    kis_undo_adapter.cpp
    kis_macro_based_undo_store.cpp
    kis_surrogate_undo_adapter.cpp
    kis_legacy_undo_adapter.cpp
    kis_post_execution_undo_adapter.cpp
    kis_processing_visitor.cpp
    kis_processing_applicator.cpp
    krita_utils.cpp
    kis_outline_generator.cpp
    kis_layer_composition.cpp
    kis_selection_filters.cpp
    KisProofingConfiguration.h
    KisRecycleProjectionsJob.cpp
    kis_selection_component.cc
    kis_keyframe.cpp
    kis_keyframe_channel.cpp
    kis_keyframe_commands.cpp
    kis_scalar_keyframe_channel.cpp
    kis_raster_keyframe_channel.cpp
    kis_onion_skin_compositor.cpp
    kis_onion_skin_cache.cpp
    kis_idle_watcher.cpp
    kis_layer_properties_icons.cpp
    KisProofingConfiguration.cpp
    kis_node_query_path.cc
    KisTranslateLayerNamesVisitor.cpp
    KisAslStorage.cpp
    KisEncloseAndFillPainter.cpp
    KisBlockBackgroundFrameGenerationLock.cpp
    KisLockFrameGenerationLock.cpp
    kis_convex_hull.cpp
)

set(einspline_SRCS
   3rdparty/einspline/bspline_create.cpp
   3rdparty/einspline/bspline_data.cpp
   3rdparty/einspline/multi_bspline_create.cpp
   3rdparty/einspline/nubasis.cpp
   3rdparty/einspline/nubspline_create.cpp
   3rdparty/einspline/nugrid.cpp
)

kis_add_library(kritaimage SHARED
    $<TARGET_OBJECTS:kritaimage_tiles3_objects>
    $<TARGET_OBJECTS:kritaimage_brushengine_objects>
    $<TARGET_OBJECTS:kritaimage_commands_objects>
    $<TARGET_OBJECTS:kritaimage_processing_objects>
    $<TARGET_OBJECTS:kritaimage_filter_objects>
    $<TARGET_OBJECTS:kritaimage_generator_objects>
    $<TARGET_OBJECTS:kritaimage_floodfill_objects>
    $<TARGET_OBJECTS:kritaimage_lazybrush_objects>
    $<TARGET_OBJECTS:kritaimage_layerstyles_objects>
    $<TARGET_OBJECTS:kritaimage_bsplines_objects>
    $<TARGET_OBJECTS:kritaimage_core_objects>
    ${__per_arch_circle_mask_generator_objs}
    ${_per_arch_processor_objs}
    ${einspline_SRCS}
)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(kritaimage_SIGNAL_HEADERS
    kis_base_node.h
    kis_paint_device.h
    brushengine/KisPaintOpPresetUpdateProxy.h
    brushengine/kis_uniform_paintop_property.h
    brushengine/kis_slider_based_paintop_property.h
)
set(kritaimage_SIGNAL_SRCS)
foreach(signal_header IN LISTS kritaimage_SIGNAL_HEADERS)
    get_filename_component(signal_name "${signal_header}" NAME_WE)
    set(signal_source "${CMAKE_CURRENT_BINARY_DIR}/${signal_name}_pk_signals.cpp")
    add_custom_command(
        OUTPUT "${signal_source}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
                -o "${signal_source}"
        DEPENDS
                "${CMAKE_SOURCE_DIR}/pk/signal/pk_signal_moc.py"
                "${CMAKE_CURRENT_SOURCE_DIR}/${signal_header}"
        COMMENT "pk_signal_moc: generating ${signal_name} signal definitions"
        VERBATIM
    )
    list(APPEND kritaimage_SIGNAL_SRCS "${signal_source}")
endforeach()

target_sources(kritaimage PRIVATE ${kritaimage_SIGNAL_SRCS})
set_target_properties(kritaimage PROPERTIES AUTOMOC OFF)

generate_export_header(kritaimage BASE_NAME kritaimage)

target_link_libraries(kritaimage
    PUBLIC
        kritaversion
        kritaglobal
        kritapsdutils
        kritapigment
        kritacommand
        kritametadata
        kritaresources
        Eigen3::Eigen
        Boost::boost
        # 判据⑨：pk 库接入主构建。PUBLIC——kritaimage 公开头引用 Pk 类型，
        # 下游消费方需经此传递获得 pk 头目录与链接。
        pkstring
        pkcontainer
        pkzip
        pkxml
        pkconfig
        pksignal
)
target_link_libraries(kritaimage PRIVATE pkconcurrent)

if(HAVE_CXX_ATOMICS_WITH_LIB OR HAVE_CXX_ATOMICS64_WITH_LIB)
   target_link_libraries(kritaimage PUBLIC atomic)
endif()

if(OpenEXR_FOUND)
  target_link_libraries(kritaimage PUBLIC ${LINK_OPENEXR_LIB})
endif()

target_link_libraries(kritaimage PRIVATE ${FFTW3_LIBRARIES})

if(APPLE)
    target_link_libraries(kritaimage PRIVATE kritamacosutils)
endif()

target_link_libraries(kritaimage PUBLIC kritamultiarch)

if (NOT GSL_FOUND)
  message (WARNING "KRITA WARNING! No GNU Scientific Library was found! Krita's Shaped Gradients might be non-normalized! Please install GSL library.")
else ()
  target_link_libraries(kritaimage PRIVATE GSL::gsl)
endif ()

target_include_directories(kritaimage
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/brushengine>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/filter>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generator>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/layerstyles>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/processing>
)

set_target_properties(kritaimage PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)
install(TARGETS kritaimage  ${INSTALL_TARGETS_DEFAULT_ARGS})
