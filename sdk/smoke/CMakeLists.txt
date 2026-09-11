# paint_smoke —— M5 验收载体（纯 C++ CLI，非 GUI）。
#
# 落点 sdk/smoke/ 依据 Ruling 3：pk/* 是独立薄壳工程（不接入 Krita 主构建），
# 而 paint_smoke 必须链 krita* 主树库；benchmarks/ 是 benchmark 的专属目录；
# sdk/ 正好是「宿主/驱动侧基础设施」（与 sdk/tests/ 并列）。
#
# 硬约束：必须链 kritaimage / kritaimpex / kritalibkra（不许写成只链 pk/* 的
# 空壳），再加 kra impex 注册与 lcms 引擎，才能真的跑通 建文档→画一笔→存 .kra。

add_executable(paint_smoke paint_smoke.cpp)

# 非 GUI ⇒ 用 ecm_mark_nongui_executable，不要 MACOSX_BUNDLE。
# 该宏定义在 ECM 的 ECMMarkNonGuiExecutable.cmake，由根 CMakeLists.txt 经
# cmake/modules/KritaAddBrokenUnitTest.cmake:7 的 include(...) 载入；
# CMake 命令（function/macro）全局可见，所以这里可直接调用。
ecm_mark_nongui_executable(paint_smoke)

# 链接闭包：
#   kritaimage / kritaimpex / kritalibkra  —— 内核三件套（硬约束，不许拿掉）。
#   kritakraimport / kritakraexport        —— kra 编解码过滤器（STATIC，直接链）。
#   kritalcmsengine                        —— 色彩空间引擎（STATIC，直接链）。
# include 路径由 kritalibkra / kritaimpex 的 PUBLIC 边传播的
# kritaimpex_noqt_compile_interface -> kritaimage_noqt_compile_interface 提供
# （libs/impex/CMakeLists.txt:34-73、libs/image/CMakeLists.txt:659-726）。
target_link_libraries(paint_smoke PRIVATE
    kritaimage
    kritaimpex
    kritalibkra
    kritakraimport
    kritakraexport
    kritalcmsengine
)

# 资源根目录：registerLcmsEngine() 要能扫描到 <树>/data/profiles/*.icc 里的内置
# ICC 剖面（p709SRGBProfile() 靠文件名 "sRGB-elle-V2-srgbtrc.icc" 查找）。
# 与测试同源：KoTestConfig.h.cmake:7 的 KRITA_RESOURCE_DIRS_FOR_TESTS 也含
# ${CMAKE_SOURCE_DIR}/data。
target_compile_definitions(paint_smoke PRIVATE
    PAINT_SMOKE_RESOURCE_DIRS="${CMAKE_SOURCE_DIR}/data"
)

if(BUILD_TESTING)
    add_test(NAME paint_smoke COMMAND paint_smoke "${CMAKE_BINARY_DIR}/paint_smoke_out.kra")
endif()
