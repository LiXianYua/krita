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
# 同一目录里其余 7 个测试虽然也报 AUTOMOC 形状的错，但那是被 AUTOMOC 错误遮住的
# 表象，真拦路虎是源码层未迁：5 个（TestKoColor、TestColorConversionSystem、
# KoRgbU8ColorSpaceTester、TestKoColorSpaceSanity、TestKoStopGradient）入口仍是
# kistest.h 的 KISTEST_MAIN（真 Qt 应用对象，S-06 Task 9 登记的 [GAP]）；另 2 个
# （TestKoColorSet、TestCompositeOpInversion）的 .cpp/.h 仍引用 Qt 名，靠
# simpletest.h 的 Qt 分支才可见。两者都属类 II 源码迁移，不在本任务范围内 —— 见任务报告。
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

