# psd_header_test / psd_colormode_block_test 已迁移到 PK_* 断言（S-05-a Task 3）：
# PK_VERIFY/PK_COMPARE 的 runner（PkTestCase::current()/checkResult 等）在 pktest
# 静态库，两个轻量测试需显式链 pktest。kis_psd_test 仍为 Qt 形态（TestUtil 真 Qt
# + flake 墙，登记缺口），不链 pktest。
set(PSD_TEST_LIBS kritapsd kritapsd_static kritatestsdk)

kis_add_test(psd_header_test.cpp
    TEST_NAME psd_header_test
    LINK_LIBRARIES kritaglobal KF${KF_MAJOR}::I18n pktest ${PSD_TEST_LIBS}
    )

kis_add_test(psd_colormode_block_test.cpp ../psd_colormode_block.cpp
    TEST_NAME psd_colormode_block_test
    LINK_LIBRARIES kritaglobal KF${KF_MAJOR}::I18n Qt${QT_MAJOR_VERSION}::Gui pktest ${PSD_TEST_LIBS}
    )

function(psd_add_pk_test_binder target source header)
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
    set_source_files_properties("${source}" PROPERTIES OBJECT_DEPENDS "${binder}")
    set_target_properties(${target} PROPERTIES AUTOMOC OFF)
    target_compile_definitions(${target} PRIVATE
        KRITA_TESTSDK_PK_NATIVE
        PK_SHELL_MOC_BINDER)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

psd_add_pk_test_binder(psd_header_test psd_header_test.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/psd_header_test.h")
psd_add_pk_test_binder(psd_colormode_block_test psd_colormode_block_test.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/psd_colormode_block_test.h")

pk_add_test(kis_psd_test
    REGISTER_FILTERS
        registerpsdImportFilter
        registerpsdExportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritapsd kritapsd_static kritaimpex
        kritapsdimport kritapsdexport
        kritalcmsengine
    )
