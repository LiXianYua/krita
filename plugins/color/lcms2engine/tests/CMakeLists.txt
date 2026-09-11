if(OpenEXR_FOUND)
    include_directories(SYSTEM ${OPENEXR_INCLUDE_DIRS})
endif()
include_directories( ${LCMS2_INCLUDE_DIR} )

# --whole-archive / --no-whole-archive 是 GNU ld 的说法，Mach-O 的 ld64 不认。
# 语义是「强制把归档里全部成员拉进来，包括没被引用的」，Mach-O 的对应物是
# -force_load,<归档绝对路径>——**不能置空**：置空会让「静态全局构造自注册」的对象
# 被链接器丢弃、构造函数不运行，静默改变行为。
#
# 关键约束：**归档仍必须以 target 名出现在链接库里**，否则 CMake 不再传播该 target
# 的 INTERFACE 链接依赖（实测：写成裸 $<TARGET_FILE:> 会丢依赖符号）。所以形制是：
# target 名留在原位，Apple 侧额外用 target_link_options 施加 -force_load，Linux 侧
# 用 --whole-archive/--no-whole-archive 夹住这些 target 名。
if(APPLE)
    set(KRITA_LCMS_WHOLE_ARCHIVE_BEGIN "")
    set(KRITA_LCMS_WHOLE_ARCHIVE_END "")
    set(KRITA_LCMS_WHOLE_ARCHIVE_OPTIONS
        "-Wl,-force_load,$<TARGET_FILE:kritalcmsengine>")
else()
    set(KRITA_LCMS_WHOLE_ARCHIVE_BEGIN "-Wl,--whole-archive")
    set(KRITA_LCMS_WHOLE_ARCHIVE_END "-Wl,--no-whole-archive")
    set(KRITA_LCMS_WHOLE_ARCHIVE_OPTIONS "")
endif()

kis_add_tests(
    TestKoLcmsColorProfile.cpp
    TestColorSpaceRegistry.cpp
    TestLcmsRGBP2020PQColorSpace.cpp
    TestProfileGeneration.cpp
    LINK_LIBRARIES kritapigment kritatestsdk ${KRITA_LCMS_WHOLE_ARCHIVE_BEGIN} kritalcmsengine ${KRITA_LCMS_WHOLE_ARCHIVE_END} ${LCMS2_LIBRARIES}
    )
# target_link_options 一次只接受一个 target（多 target 形报 invalid arguments），
# 而 kis_add_tests 复数形展开成 4 个 add_executable，故逐个施加。
foreach(_lcms_pk_target
        TestKoLcmsColorProfile TestColorSpaceRegistry
        TestLcmsRGBP2020PQColorSpace TestProfileGeneration)
    target_link_options(${_lcms_pk_target} PRIVATE ${KRITA_LCMS_WHOLE_ARCHIVE_OPTIONS})
endforeach()

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
