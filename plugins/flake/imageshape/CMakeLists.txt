project(imageshape)

set ( ImageShape_SRCS
    ImageShapePlugin.cpp
    ImageShape.cpp
    ImageShapeState.cpp
    ImageShapeFactory.cpp
)

kis_add_library(krita_shape_image STATIC ${ImageShape_SRCS})
set_target_properties(krita_shape_image PROPERTIES AUTOMOC OFF)

target_include_directories(krita_shape_image PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/xml
    # A4 判据⑧ 两侧一致：ImageShape.h:13 的 <PkPainter.h> 属 pk/render。本 target 原先
    # 是从 kritaflake 的 PUBLIC include 面（libs/flake/CMakeLists.txt:601
    # `$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/render>`）间接拿到 pk/render 的；这里
    # 把同一目录显式声明一次，来源可逐条对照。CMake 对相同 include 目录去重，故编译行上
    # `-I…/pk/render` 的出现次数不变（实测 1 次）。
    ${CMAKE_SOURCE_DIR}/pk/render
)

target_link_libraries(krita_shape_image
    kritaflake
    kritaflake_imagecodec
    pkstring
    pkcontainer
    pkvariant
    pkgeometry
    pkimage
    pkxml
    PNG::PNG
    TIFF::TIFF
)
if(JPEG_FOUND)
    target_link_libraries(krita_shape_image JPEG::JPEG)
    target_compile_definitions(krita_shape_image PRIVATE HAVE_JPEG=1)
else()
    target_compile_definitions(krita_shape_image PRIVATE HAVE_JPEG=0)
endif()

install(TARGETS krita_shape_image DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
