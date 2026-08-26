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

kis_add_test(kis_psd_test.cpp
    TEST_NAME kis_psd_test
    LINK_LIBRARIES ${PSD_TEST_LIBS} kritaimpex
    )
