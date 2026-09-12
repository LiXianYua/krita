add_executable(CCSGraph CCSGraph.cpp)
target_link_libraries(CCSGraph kritapigment KF${KF_MAJOR}::I18n)
ecm_mark_as_test(CCSGraph)


kis_add_tests(
    TestKoColorSet.cpp
    TestKoColorSpaceRegistry.cpp
    TestKoColorSpaceAbstract.cpp
    TestColorConversionSystem.cpp
    TestKoColor.cpp
    TestKoIntegerMaths.cpp
    TestConvolutionOpImpl.cpp
    KoRgbU8ColorSpaceTester.cpp
    TestKoColorSpaceSanity.cpp
    TestFallBackColorTransformation.cpp
    TestKoChannelInfo.cpp

    LINK_LIBRARIES kritapigment KF${KF_MAJOR}::I18n kritatestsdk
    )

kis_add_test(
    TestCompositeOpInversion.cpp TestCompositeOpInversion_p.cpp
    TEST_NAME TestCompositeOpInversion
    LINK_LIBRARIES kritapigment KF${KF_MAJOR}::I18n kritatestsdk
    )

kis_add_tests(
    TestColorConversion.cpp
    TestKoColorSpaceMaths.cpp
    TestKisSwatchGroup.cpp
    TestKoStopGradient.cpp

    LINK_LIBRARIES kritapigment kritatestsdk
    )


# pk 测试发现：pk_test_moc.py 为每个 Pk 形态测试头生成 PkTestBinder<T> 特化
# （产物是 .inc，由调用 qExec<T> 的那个 .cpp 在同一个 TU 里 #include —— ODR
# 契约见 pk/test/CMakeLists.txt）。形制照抄
# libs/psdutils/tests/CMakeLists.txt 的 psdutils_add_pk_test_binder。
#
# 下列测试类此前已迁 Pk（派生 PkTestObject，Q_OBJECT 走 pk 垫片形态），但 Qt
# AUTOMOC 仍在跑：真 moc 为没有 staticMetaObject/qt_static_metacall 声明的类
# 生成元对象代码 ⇒ "out-of-line definition ... does not match any declaration"。
# 所以这一支必须关掉 AUTOMOC，测试发现改由 pk_test_moc.py 承担。
function(pigment_add_pk_test_binder target header)
    get_filename_component(header_base "${header}" NAME_WE)
    set(binder "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_${header_base}.inc")
    add_custom_command(
        OUTPUT "${binder}"
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_SOURCE_DIR}/pk/test/pk_test_moc.py"
                "${header}"
                -o "${binder}"
        DEPENDS
            "${header}"
            "${CMAKE_SOURCE_DIR}/pk/test/pk_test_moc.py"
        COMMENT "pk_test_moc ${header}"
        VERBATIM)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/${target}.cpp"
        PROPERTIES OBJECT_DEPENDS "${binder}")
    set_target_properties(${target} PROPERTIES AUTOMOC OFF)
    target_compile_definitions(${target} PRIVATE
        KRITA_TESTSDK_PK_NATIVE
        PK_SHELL_MOC_BINDER)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

# 覆盖「AUTOMOC 是唯一拦路虎」的 target —— 即关掉 AUTOMOC、接上生成器之后就能
# 真正编出并跑通的那些。M5 轮 3 Task B 的 B5 又收进两个：TestKoIntegerMaths 与
# TestKisSwatchGroup（它们的源码层残留一并迁了：qAbs→pkAbs 见 TestKoIntegerMaths.cpp、
# Qt::blue/red→Pk::blue/red 见 TestKisSwatchGroup.cpp、pair 哈希重载由 qHash 换成
# Pk 唯一的扩展点 pkHash 见 TestKisSwatchGroup.h），7 + 2 = 9 个。
#
# 同一目录里其余 7 个测试的拦路虎**不止一种**：5 个（TestKoColor、TestColorConversionSystem、
# KoRgbU8ColorSpaceTester、TestKoColorSpaceSanity、TestKoStopGradient）入口仍是
# kistest.h 的 KISTEST_MAIN（真 Qt 应用对象，S-06 Task 9 登记的 [GAP]）；另 2 个
# （TestKoColorSet、TestCompositeOpInversion）的 .cpp/.h 仍引用 Qt 名，靠
# simpletest.h 的 Qt 分支才可见。两者都属类 II 源码迁移，不在本任务范围内 —— 见任务报告。
#
# R-65（Task 2+4）实测订正上面这段：**「真拦路虎是源码层未迁」这条只对入口那一层成立，
# AUTOMOC 不是「被遮住的表象」，是独立且真实的一类。** 以 TestColorConversionSystem 为例，
# `ninja -C build-r65 TestColorConversionSystem` 现场量到的是**四类并列**的拦路虎
# （原始输出 1491 行：
# .superpowers/sdd/R-65/evidence/t24/r61-TestColorConversionSystem-ninja.log）：
#   ① 入口 KISTEST_MAIN —— TestColorConversionSystem.cpp:499:1
#      `error: no matching function for call to 'qExec'`（KISTEST_MAIN 宏展开没落点）；
#      其 :16 `#include <kistest.h>` 在 pk 栈上断得更早：
#      `sdk/tests/kistest.h:21:10: fatal error: 'QApplication' file not found`
#   ② QDebug + PkString —— TestColorConversionSystem.cpp:53:57 与
#      libs/pigment/KoColorConversionSystem_p.h:48:37
#      `error: invalid operands to binary expression ('QDebug' and 'const PkString')`
#   ③ AUTOMOC —— 同一个失败的 TU 里，
#      build-r65/libs/pigment/tests/TestColorConversionSystem_autogen/EWIEGA46WW/
#      moc_TestColorConversionSystem.cpp:99:33
#      `error: out-of-line definition of 'qt_static_metacall' does not match any
#       declaration in 'TestColorConversionSystem'`
#      （另含 qt_metacast / qt_metacall / staticMetaObject / `static_cast from 'QObject *'
#       to 'TestColorConversionSystem *'` 等共 12 条同类错误，全在这一条 TU 上）
#   ④ Qt::GlobalColor → PkString —— TestColorConversionSystem.cpp:371:42 / :377:42 /
#      :385:42 / :391:42
#      `error: conversion from 'Qt::GlobalColor' to 'const PkString' is ambiguous`
# 结论：对这几个 target，「关掉 AUTOMOC + 接上 pk_test_moc.py」**不足以**编过——①在入口层、
# ②④在源码层，三类要分别处置，AUTOMOC 只是其中一类。
#
# R-65 另量到（同一轮，见 .superpowers/sdd/R-65/task-24-report.md §交接 R-61）：TestKoColorSet
# 与 TestCompositeOpInversion 的源码**已经**迁到 pk 名（在本目录下用 pk_add_test 注册后
# ninja exit=0，两产物都编出来了），但仍不绿：运行期 SEGFAULT，根因是 pk 栈里没有
# lcms 色彩空间引擎的静态注册（plugins/color/lcms2engine 的 registerLcmsEngine()，
# 见 libs/pigment/KoColorSpaceRegistry.cpp:36-40、:198-221 的弱引用守卫），于是
# KoColorSpaceRegistry::colorSpace(...) 返回 0x0，KoColorSet.cpp:2410 与
# TestCompositeOpInversion_p.cpp:400 裸解引用。归属 S-09/S-04。故本轮不注册这两个。

foreach(_pigment_pk_target
        TestKoColorSpaceRegistry TestKoColorSpaceAbstract
        TestConvolutionOpImpl
        TestFallBackColorTransformation TestKoChannelInfo
        TestColorConversion TestKoColorSpaceMaths
        TestKoIntegerMaths TestKisSwatchGroup)
    pigment_add_pk_test_binder(${_pigment_pk_target}
        "${CMAKE_CURRENT_SOURCE_DIR}/${_pigment_pk_target}.h")
endforeach()


macos_test_fixrpath(CCSGraph)


# R-61 Task 2 降级 driver（R线-spec「依赖墙挡住真实测试类时：允许降级成复刻调用点
# 形状的 driver」）：逐行复刻 TestColorConversionSystem 里 p709SRGBProfile() 的调用点
# 形状（三元组构造 + existsPath/existsGoodPath + calcPath 的 findBestPath 走法）。
# 真实测试类本树编不过（四类源码层阻塞，见该 .cpp 头注释与 plan §1 F4），故用
# plain main() driver 落常驻证据。零 Qt：只链 kritapigment + kritalcmsengine，
# include 目录只要 lcms 引擎头（registerLcmsEngine）。
kis_add_test(
    PkP709SRgbProfileNoResourceDirsDriver.cpp
    LINK_LIBRARIES kritapigment kritalcmsengine
)
target_include_directories(PkP709SRgbProfileNoResourceDirsDriver PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/color/lcms2engine
)


# R-72 第三处落点降级 driver（R线-spec「依赖墙挡住真实测试类时：允许降级成复刻调用点
# 形状的 driver」）：逐行复刻 TestKoColor::testExistingSerializations
# （libs/pigment/tests/TestKoColor.cpp:68–87）里 p2020G10Profile()->name() 的调用点形状，
# 并把 :81 的 PK_VERIFY2 可达性前提打印出来（`:85` 是否可达由实测决定）。
# 真实测试类本树编不过（AUTOMOC 元对象 + QDebug/PkString + Qt::GlobalColor + KoColor 构造 +
# qExec 五类源码层阻塞，见该 .cpp 头注释），故用 plain main() driver 落常驻证据。
# 零 Qt：只链 kritapigment + kritalcmsengine，include 目录只要 lcms 引擎头（registerLcmsEngine）。
kis_add_test(
    PkKoColorP2020ProfileNoResourceDirsDriver.cpp
    LINK_LIBRARIES kritapigment kritalcmsengine
)
target_include_directories(PkKoColorP2020ProfileNoResourceDirsDriver PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/color/lcms2engine
)

