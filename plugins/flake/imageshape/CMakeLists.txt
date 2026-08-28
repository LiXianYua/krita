project(imageshape)

set ( ImageShape_SRCS
    ImageShapePlugin.cpp
    ImageShape.cpp
    ImageShapeState.cpp
    ImageShapePngData.cpp
    ImageShapeFactory.cpp
)

kis_add_library(krita_shape_image MODULE ${ImageShape_SRCS})
set_target_properties(krita_shape_image PROPERTIES AUTOMOC OFF)

target_include_directories(krita_shape_image PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/xml
)

target_link_libraries(krita_shape_image
    kritaflake
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
