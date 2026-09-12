set(imageshape_cow_test_SRCS
    ImageShapeCowTest.cpp
    ../ImageShape.cpp
    ../ImageShapeState.cpp
    ${CMAKE_SOURCE_DIR}/libs/flake/shapes/ImageShapePngData.cpp
    ../ImageShapeFactory.cpp
    ../ImageShapePlugin.cpp
)

kis_add_test(${imageshape_cow_test_SRCS}
    TEST_NAME ImageShapeCowTest
    # R-74：ImageShape.cpp 的 paint(PkPainter&) 调 PkPainter::drawImage/
    # setClipRect/setTransform，实现库是 pkrender（pk/render/CMakeLists.txt:12）；
    # 不链它，链接期 PkPainter::drawImage 未定义。
    LINK_LIBRARIES pkstring pkcontainer pkgeometry pkimage pkvariant pkxml pkrender PNG::PNG TIFF::TIFF
)
set_target_properties(ImageShapeCowTest PROPERTIES AUTOMOC OFF)
target_include_directories(ImageShapeCowTest BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/namespace
    # 本 target 直接编 ../ImageShape.cpp / ../ImageShapeState.cpp，它们的
    # ImageShape.h:13 是 <PkPainter.h>（pk/render/PkPainter.h）。主树
    # krita_shape_image 是从 kritaflake 的 PUBLIC include 面传递拿到 pk/render 的
    # （libs/flake/CMakeLists.txt:601），本 target 不链 kritaflake，
    # 所以要显式进 include 面。
    ${CMAKE_SOURCE_DIR}/pk/render
)

kis_add_test(ImageShapeSerializationTest.cpp ${CMAKE_SOURCE_DIR}/libs/flake/shapes/ImageShapePngData.cpp
    TEST_NAME ImageShapeSerializationTest
    LINK_LIBRARIES pkstring pkcontainer pkgeometry pkimage pkvariant PNG::PNG TIFF::TIFF
)
set_target_properties(ImageShapeSerializationTest PROPERTIES AUTOMOC OFF)
target_compile_definitions(ImageShapeSerializationTest PRIVATE IMAGESHAPE_CODEC_TESTING=1)

foreach(image_shape_test ImageShapeCowTest ImageShapeSerializationTest)
    if(JPEG_FOUND)
        target_link_libraries(${image_shape_test} JPEG::JPEG)
        target_compile_definitions(${image_shape_test} PRIVATE HAVE_JPEG=1)
    else()
        target_compile_definitions(${image_shape_test} PRIVATE HAVE_JPEG=0)
    endif()
endforeach()
