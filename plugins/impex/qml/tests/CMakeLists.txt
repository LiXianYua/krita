pk_add_test(
    KisQmlTest
    REGISTER_FILTERS
        registerQMLExportFilter
    REGISTER_ENGINES
        registerLcmsEngine
    LINK_LIBRARIES
        kritaimpex
        kritaqmlexport
        kritalcmsengine
    )

add_executable(KisQmlFormatTest
    KisQmlFormatTest.cpp
    ../qml_format.cc
)
target_include_directories(KisQmlFormatTest PRIVATE
    ..
)
target_link_libraries(KisQmlFormatTest PRIVATE
    kritastore pkgeometry pkport pkstring pkvariant
    pktextstream
)
add_test(NAME KisQmlFormatTest COMMAND KisQmlFormatTest)
