if(OpenEXR_FOUND)
    include_directories(SYSTEM ${OPENEXR_INCLUDE_DIRS})
endif()
include_directories( ${LCMS2_INCLUDE_DIR} )

kis_add_tests(
    TestKoLcmsColorProfile.cpp
    TestColorSpaceRegistry.cpp
    TestLcmsRGBP2020PQColorSpace.cpp
    TestProfileGeneration.cpp
    LINK_LIBRARIES kritapigment kritatestsdk ${LCMS2_LIBRARIES}
    )

add_executable(kritalcms_profile_discovery_test
    TestLcmsProfileDiscovery.cpp
)
set_target_properties(kritalcms_profile_discovery_test PROPERTIES AUTOMOC OFF)
target_include_directories(kritalcms_profile_discovery_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../colorprofiles
)
target_link_libraries(kritalcms_profile_discovery_test PRIVATE
    pkcontainer
    pkstring
)
add_test(
    NAME plugins-color-lcms2engine-profile-discovery
    COMMAND kritalcms_profile_discovery_test
)
if (UNIX)
    add_test(
        NAME plugins-color-lcms2engine-home-unset
        COMMAND ${CMAKE_COMMAND} -E env --unset=HOME
                $<TARGET_FILE:kritalcms_profile_discovery_test> --home-unset
    )
endif()

if (KRITA_USE_SURFACE_COLOR_MANAGEMENT_API)
    kis_add_test(
        TestIccFromColorimetryConversion.cpp
        LINK_LIBRARIES kritapigment kritatestsdk ${LCMS2_LIBRARIES} kritasurfacecolormanagementapi
    )
endif()
