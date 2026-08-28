kis_add_test(
    KisQmlTest.cpp
    TEST_NAME KisQmlTest
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(KisQmlFormatTest
    KisQmlFormatTest.cpp
    ../qml_format.cc
    ${CMAKE_SOURCE_DIR}/libs/pigment/PkTextStream.cpp
)
target_include_directories(KisQmlFormatTest PRIVATE
    ..
    ${CMAKE_SOURCE_DIR}/libs/pigment
)
target_link_libraries(KisQmlFormatTest PRIVATE
    kritastore pkgeometry pkport pkstring pkvariant
)
add_test(NAME KisQmlFormatTest COMMAND KisQmlFormatTest)
