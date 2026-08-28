########### next target ###############

kis_add_test(
    kis_mypaintop_test.cpp ../MyPaintPaintOpSettings.cpp ../MyPaintPaintOpPreset.cpp ../MyPaintSurface.cpp
    TEST_NAME KisMyPaintOpTest
    LINK_LIBRARIES kritaimage kritamypaintop_static kritalibpaintop LibMyPaint::mypaint kritatestsdk pkimageio
    )

kis_add_test(
    MyPaintJsonTest.cpp ../MyPaintJson.cpp
    TEST_NAME MyPaintJsonTest
    LINK_LIBRARIES json-c::json-c
    )
