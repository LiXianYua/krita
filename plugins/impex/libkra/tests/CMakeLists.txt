# --whole-archive / --no-whole-archive 是 GNU ld 的说法，Mach-O 的 ld64 不认
# （`ld: unknown options: --whole-archive`）。语义是「强制拉入归档全部成员」，Mach-O
# 的对应物是 -force_load,<归档绝对路径>——**不能置空**（置空会让靠静态全局构造自注册的
# 对象被链接器丢弃、构造函数不运行）。形制照 plugins/generators/seexpr/tests/CMakeLists.txt
# （M5 Task 2c）与 libs/image/tests/CMakeLists.txt（S-10）：
# **归档仍以 target 名出现在链接行里**（否则 CMake 不再传播它的 INTERFACE 依赖），
# Apple 侧额外用 target_link_options 施加 -force_load。
# 来源：M5 全分支评审 F1（2026-09-12）——这两处当时被更早的编译失败挡着，所以没暴露。
if(APPLE)
    set(KRA_TESTS_WHOLE_ARCHIVE_BEGIN "")
    set(KRA_TESTS_WHOLE_ARCHIVE_END "")
else()
    set(KRA_TESTS_WHOLE_ARCHIVE_BEGIN "-Wl,--whole-archive")
    set(KRA_TESTS_WHOLE_ARCHIVE_END "-Wl,--no-whole-archive")
endif()

pk_add_test(
    kis_kra_loader_test
    REGISTER_FILTERS
        registerKraImportFilter
        registerKraExportFilter
    LINK_LIBRARIES
        kritaimpex kritalibkra
        kritakraimport kritakraexport kritalcmsengine
        ${KRA_TESTS_WHOLE_ARCHIVE_BEGIN} kritaexample ${KRA_TESTS_WHOLE_ARCHIVE_END}
    )
if(APPLE)
    target_link_options(kis_kra_loader_test PRIVATE
        "-Wl,-force_load,$<TARGET_FILE:kritaexample>")
endif()

kis_add_test(
    kis_file_layer_roundtrip_test.cpp
    LINK_LIBRARIES kritaimpex kritalibkra kritatestsdk
    )


pk_add_test(
    kis_kra_saver_test
    REGISTER_FILTERS
        registerKraImportFilter
        registerKraExportFilter
    LINK_LIBRARIES
        kritaimpex kritalibkra $<LINK_ONLY:kritatransformmaskstubs>
        kritakraimport kritakraexport kritalcmsengine
        ${KRA_TESTS_WHOLE_ARCHIVE_BEGIN}
            $<LINK_ONLY:kritaexample> $<LINK_ONLY:kritapixelizefilter>
            $<LINK_ONLY:kritacolorgenerator> $<LINK_ONLY:kritapatterngenerator>
        ${KRA_TESTS_WHOLE_ARCHIVE_END}
    )
if(APPLE)
    target_link_options(kis_kra_saver_test PRIVATE
        "-Wl,-force_load,$<TARGET_FILE:kritaexample>"
        "-Wl,-force_load,$<TARGET_FILE:kritapixelizefilter>"
        "-Wl,-force_load,$<TARGET_FILE:kritacolorgenerator>"
        "-Wl,-force_load,$<TARGET_FILE:kritapatterngenerator>")
endif()

# R-82：`SYSTEM_RESOURCES_DATA_DIR` 原本由 `kritatestsdk`（Qt 测试栈）的 INTERFACE
# 编译定义提供（`sdk/tests/CMakeLists.txt:10-13`），切到 `kritatestsdk_pk` 之后就没有了
# ——而 `kis_kra_saver_test.cpp:79`（`initTestCase()`，函数体一字未动）要用它去
# `KoResourcePaths::addAssetDir(ResourceType::Patterns, …)` 挂 Patterns 资源目录。
# 这里按**同一个值、同一个口径**补回来（Qt 栈的定义就是 `${CMAKE_SOURCE_DIR}/data/`），
# 只补给这一条 target，不动 `kritatestsdk_pk` 的共享面。
target_compile_definitions(kis_kra_saver_test PRIVATE
    SYSTEM_RESOURCES_DATA_DIR="${CMAKE_SOURCE_DIR}/data/"
)

# R-82：原来这里有一套手写的 `libkra_add_pk_test_binder()`（add_custom_command +
# OBJECT_DEPENDS + 手动 KRITA_TESTSDK_PK_NATIVE/PK_SHELL_MOC_BINDER）。两条 target
# 现在都改由 `pk_add_test` 注册，**它自带等价的 binder 机制**（生成
# `pk_binder_<target>.inc` 并作为 wrap TU 的 OBJECT_DEPENDS；测试源里
# `#ifdef PK_SHELL_MOC_BINDER` 那段因此不再激活 —— 若同时生效会把同一份
# `PkTestBinder<T>` 特化编两遍、撞重复特化）。故整套删除。
# 四个额外 `-force_load`（kritaexample / kritapixelizefilter /
# kritacolorgenerator / kritapatterngenerator）**保留**，见上面的 target_link_options。
