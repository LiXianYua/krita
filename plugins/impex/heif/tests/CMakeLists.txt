if(APPLE)
    krita_add_broken_unit_test(
        KisHeifTest.cpp
        TEST_NAME KisHeifTest
        LINK_LIBRARIES kritaimpex kritatestsdk
    )

else(APPLE)

    kis_add_test(
        KisHeifTest.cpp
        TEST_NAME KisHeifTest
        LINK_LIBRARIES kritaimpex kritatestsdk
    )


endif()

add_executable(heif_validation_probe heif_validation_probe.cpp)
target_link_libraries(heif_validation_probe PRIVATE HEIF::heif)
add_test(NAME heif_validation_probe COMMAND heif_validation_probe)

# R-72 Task 2 降级 driver（R线-spec「依赖墙挡住真实测试类时：允许降级成复刻调用点
# 形状的 driver」）：逐行复刻 `HeifImport::convert`（`plugins/impex/heif/HeifImport.cpp:226`）
# 里 `profileFor(...)` → `profile->name()`（`:356`/`:365`，nclx 分支）的调用点形状。
# 真实测试类本树不可用（R-72 plan §1 F5）：`KisHeifTest` 在 APPLE 上是
# `krita_add_broken_unit_test`（本文件 `:1-6`），且真导入还需 libheif 句柄与一份带
# nclx（primaries=BT.709、transfer=IEC_61966_2_1）的真 `.heic` 样本文件，故用
# plain main() driver 落常驻证据。零 Qt：只链 kritapigment + kritalcmsengine，
# include 目录只要 lcms 引擎头（registerLcmsEngine）。
# 这堵墙什么时候拆：需要可用的 heif 导入测试（R-65 收口测试侧 target）+ 上述样本文件。
kis_add_test(
    PkHeifProfileForNoResourceDirsDriver.cpp
    LINK_LIBRARIES kritapigment kritalcmsengine
)
target_include_directories(PkHeifProfileForNoResourceDirsDriver PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/color/lcms2engine
)
