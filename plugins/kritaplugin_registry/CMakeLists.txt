# D-12 静态注册：聚合器目标。链入全部插件静态库，由 registerAllPlugins() 统一注册。
# 自注册型插件（paintops 等用静态全局对象触发注册）需 --whole-archive 强制包含，
# 否则链接器丢弃无引用目标，其全局构造函数不会运行。
set(_registry_srcs ${CMAKE_CURRENT_SOURCE_DIR}/../register_all_plugins.cpp)
kis_add_library(kritaplugin_registry STATIC ${_registry_srcs})
target_include_directories(kritaplugin_registry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# --whole-archive / --no-whole-archive 是 GNU ld 的说法，Mach-O 的 ld64 不认
# （`ld: unknown options: --whole-archive`）。这里**不能置空**——本文件开头的注释已经
# 写明「自注册型插件需强制包含，否则链接器丢弃无引用目标、其全局构造函数不会运行」。
# Mach-O 的对应物是 -force_load,<归档绝对路径>；形制照
# plugins/generators/seexpr/tests/CMakeLists.txt（M5 Task 2c）：
# **归档仍以 target 名出现在 target_link_libraries 里**（否则 CMake 不再传播它们的
# INTERFACE 依赖），Apple 侧额外用 target_link_options 逐个施加 -force_load。
# 来源：M5 全分支评审 F1（2026-09-12）。
set(KRITAPLUGIN_REGISTRY_LIBS
    kritawebpexport kritawebpimport kritargbeimport kritaasccdl kritaassistanttool kritablurfilter kritabrushexport kritabrushimport kritacolorgenerator kritacolorsfilters kritacolorsmudgepaintop krita_colorspaces_extensions kritaconvertheighttonormalmap kritaconvolutionfilters kritacsvexport kritacsvimport kritacurvepaintop kritadefaultpaintops kritadefaulttools kritadeformpaintop kritadodgeburn kritaedgedetection kritaembossfilter kritaexample kritaexif kritaexperimentpaintop kritaexrexport kritaexrimport kritaextensioncolorsfilters kritafastcolortransferfilter kritafilterop krita_flaketools kritagaussianhighpassfilter kritagifexport kritagifimport kritagradientgenerator kritagradientmap kritagridpaintop kritahairypaintop kritahalftone kritahatchingpaintop kritaheifexport kritaheifimport kritaheightmapexport kritaheightmapimport kritaimageenhancement kritaindexcolors kritaiptc kritajp2import kritajpegexport kritajpegimport kritajxlexport kritajxlimport krita_karbontools kritakraexport kritakraimport kritakrzexport kritalcmsengine kritalevelfilter kritamultigridpatterngenerator kritamypaintop kritanoisefilter kritanormalize kritaoilpaintfilter kritaoraexport kritaoraimport kritapalettize kritaparticlepaintop kritapatterngenerator kritapdfimport kritaphongbumpmap kritapixelizefilter kritapngexport kritapngimport kritaposterize kritapropagatecolors kritapsdexport kritapsdimport kritaqimageioexport kritaqimageioimport kritaqmlexport kritaraindropsfilter kritarandompickfilter krita_raw_import kritaresettransparent kritargbeexport kritaroundcornersfilter kritaroundmarkerpaintop kritascreentonegenerator kritaseexprgenerator kritaselectiontools krita_shape_paths kritasimplexnoisegenerator kritasketchpaintop kritasmalltilesfilter kritaspraypaintop kritaspriterexport kritasvgimport kritatangentnormalpaintop kritatgaexport kritatgaimport kritathreshold kritatiffexport kritatiffimport kritatoolcrop kritatooldyna kritatoolencloseandfill kritatoolKnife kritatoollazybrush kritatoolpolygon kritatoolpolyline kritatoolSmartPatch krita_tool_svgtext kritatooltransform kritaunsharpfilter kritawavefilter kritaxcfimport kritaxmp)

if(APPLE)
    target_link_libraries(kritaplugin_registry PRIVATE ${KRITAPLUGIN_REGISTRY_LIBS})
    foreach(_registry_lib IN LISTS KRITAPLUGIN_REGISTRY_LIBS)
        target_link_options(kritaplugin_registry PRIVATE
            "-Wl,-force_load,$<TARGET_FILE:${_registry_lib}>")
    endforeach()
else()
    target_link_libraries(kritaplugin_registry PRIVATE
        -Wl,--whole-archive ${KRITAPLUGIN_REGISTRY_LIBS} -Wl,--no-whole-archive)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
