# ---------------------------------------------------------------------------
# R-82：本 target 从 Qt 测试栈（kritatestsdk）切到 pk 测试栈（pk_add_test →
# kritatestsdk_pk），并**把本格式的两个 filter 注册对象强制拉进链接**。
#
# 为什么必须拉（判据不是「编过」而是「跑绿」）：决策 D-12 之后插件是 STATIC +
# 静态全局自注册，链接器只按符号引用取归档成员 —— 没有引用的那个 .o 整份丢掉
# ⇒ `registerKisTGAImportFilter()` / `registerKisTGAExportFilter()` 从不被调用
# ⇒ `KisImportExportManager::registerFilter()` 写的那张函数内静态表**恒空**
# （libs/impex/KisImportExportManager.cpp:119-131）。此时：
#   * KisTgaTest 的三个用例断言 `!status.isOk()`，而「一个 filter 都没有」天然
#     给出失败状态 ⇒ 它们会**假绿**；
#   * 正是 `R线-spec` 五种假绿里「绿名单不能从『没观察到红』推出来」那一族。
#
# 拉法用 **`-u <注册入口符号>`**，不是 `-force_load <整档>`：本格式的
# `tga_validation.cpp` **同时**编进了 kritatgaimport 与 kritatgaexport 两个归档，
# 整档 force_load 会撞 `duplicate symbol`（实测：`tgaDestinationX` /
# `validateTgaExportDimensions`）。`-u` 只拉含该符号的那**一个**成员
# （`kis_tga_import.cpp.o` / `kis_tga_export.cpp.o`），精确且无重复。
# 这也是本仓库既有的机制：`plugins/impex/tests/kis_impex_static_registration_test.cpp`
# 就是靠对同一批 `extern "C"` 入口的**弱引用**把各 codec 拉进来的
# （见该文件 :20-60 的 `extern "C" … KIS_WEAK_REGISTRATION` 声明）。
# 入口符号名可从归档直接读出来（`nm -g build-*/lib/libkritatgaimport.a | grep -i register`
# → `T _registerKisTGAImportFilter`）。
# ---------------------------------------------------------------------------
# 拉法用 pk_add_test 的 **REGISTER_FILTERS**（不是 `-force_load <整档>`，也不是
# `-u <符号>`）：整档 force_load 会撞 `duplicate symbol` —— 本格式的
# `tga_validation.cpp` **同时**编进了 kritatgaimport 与 kritatgaexport 两个归档
# （实测重复符号：`tgaDestinationX` / `validateTgaExportDimensions`）；
# 而 `-u` 只把对象拉进来、**没有人调用那个入口**，注册表照样是空的
# （实测：KisTgaTest 的 testImportIncorrectFormat 拿到 `FileFormatNotSupported`）。
# REGISTER_FILTERS 生成一个直接编进本 target 的 TU：它既引用入口（拉成员）、
# 又在静态初始化期真的调用它们 —— 两个问题一次解决，且粒度精确到本格式。
# 入口符号名可从归档直接读出来：
#   `nm -g build-*/lib/libkritatgaimport.a | grep -i register`
#   → `T _registerKisTGAImportFilter`
# 机制说明与判别力对照物见 sdk/tests/pk_test_support.cmake 的 REGISTER_FILTERS 段。
pk_add_test(
    KisTgaTest
    REGISTER_FILTERS
        registerKisTGAImportFilter
        registerKisTGAExportFilter
    LINK_LIBRARIES
        kritaimpex
        kritatgaimport kritatgaexport
    )

add_executable(tga_validation_probe tga_validation_probe.cpp ../tga_validation.cpp)
target_include_directories(tga_validation_probe PRIVATE ${CMAKE_SOURCE_DIR}/pk/global)
add_test(NAME tga_validation_probe COMMAND tga_validation_probe)
