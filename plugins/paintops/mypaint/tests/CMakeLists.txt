########### next target ###############

kis_add_test(
    kis_mypaintop_test.cpp ../MyPaintPaintOpSettings.cpp ../MyPaintPaintOpPreset.cpp ../MyPaintSurface.cpp
    TEST_NAME KisMyPaintOpTest
    LINK_LIBRARIES kritaimage kritamypaintop_static kritalibpaintop LibMyPaint::mypaint kritatestsdk pkimageio
    )

set(mypaint_test_binder
    "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_kis_mypaintop_test.inc")
add_custom_command(
    OUTPUT "${mypaint_test_binder}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/pk/test/pk_test_moc.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/kis_mypaintop_test.h"
            -o "${mypaint_test_binder}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/kis_mypaintop_test.h"
        "${CMAKE_SOURCE_DIR}/pk/test/pk_test_moc.py"
    COMMENT "pk_test_moc kis_mypaintop_test.h"
    VERBATIM)
set_source_files_properties(kis_mypaintop_test.cpp PROPERTIES
    OBJECT_DEPENDS "${mypaint_test_binder}")
set_target_properties(KisMyPaintOpTest PROPERTIES AUTOMOC OFF)
target_include_directories(KisMyPaintOpTest PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

kis_add_test(
    MyPaintJsonTest.cpp ../MyPaintJson.cpp
    TEST_NAME MyPaintJsonTest
    LINK_LIBRARIES json-c::json-c
    )
