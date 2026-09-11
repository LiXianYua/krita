# --whole-archive / --no-whole-archive 是 GNU ld 的说法，Mach-O 的 ld64 不认
# （`ld: unknown options: --whole-archive`）。语义是「强制拉入归档全部成员」，Mach-O
# 的对应物是 -force_load,<归档绝对路径>——**不能置空**（置空会让靠静态全局构造自注册的
# 对象被链接器丢弃、构造函数不运行）。形制照 plugins/generators/seexpr/tests/CMakeLists.txt
# （M5 Task 2c）与 libs/image/tests/CMakeLists.txt（S-10）：
# **归档仍以 target 名出现在链接行里**（否则 CMake 不再传播它的 INTERFACE 依赖），
# Apple 侧额外用 target_link_options 施加 -force_load。
# 来源：M5 全分支评审 F1（2026-09-12）——这两处当时被更早的编译失败挡着，所以没暴露。
if(APPLE)
    set(KRA_TESTS_WHOLE_ARCHIVE_BEGIN "")
    set(KRA_TESTS_WHOLE_ARCHIVE_END "")
else()
    set(KRA_TESTS_WHOLE_ARCHIVE_BEGIN "-Wl,--whole-archive")
    set(KRA_TESTS_WHOLE_ARCHIVE_END "-Wl,--no-whole-archive")
endif()

kis_add_tests(
    kis_kra_loader_test.cpp
    LINK_LIBRARIES
        kritaimpex kritalibkra kritatestsdk
        kritakraimport kritakraexport kritalcmsengine
        ${KRA_TESTS_WHOLE_ARCHIVE_BEGIN} kritaexample ${KRA_TESTS_WHOLE_ARCHIVE_END}
    )
if(APPLE)
    target_link_options(kis_kra_loader_test PRIVATE
        "-Wl,-force_load,$<TARGET_FILE:kritaexample>")
endif()

kis_add_test(
    kis_file_layer_roundtrip_test.cpp
    LINK_LIBRARIES kritaimpex kritalibkra kritatestsdk
    )


kis_add_test(
    kis_kra_saver_test.cpp
    LINK_LIBRARIES
        kritaimpex kritalibkra kritatransformmaskstubs
        kritakraimport kritakraexport kritalcmsengine
        ${KRA_TESTS_WHOLE_ARCHIVE_BEGIN}
            kritaexample kritapixelizefilter
            kritacolorgenerator kritapatterngenerator
        ${KRA_TESTS_WHOLE_ARCHIVE_END}
    )
if(APPLE)
    target_link_options(kis_kra_saver_test PRIVATE
        "-Wl,-force_load,$<TARGET_FILE:kritaexample>"
        "-Wl,-force_load,$<TARGET_FILE:kritapixelizefilter>"
        "-Wl,-force_load,$<TARGET_FILE:kritacolorgenerator>"
        "-Wl,-force_load,$<TARGET_FILE:kritapatterngenerator>")
endif()

function(libkra_add_pk_test_binder target source header)
    get_filename_component(header_base "${header}" NAME_WE)
    set(binder "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_${header_base}.inc")
    add_custom_command(
        OUTPUT "${binder}"
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_SOURCE_DIR}/pk/test/pk_test_moc.py"
                "${header}"
                -o "${binder}"
        DEPENDS
            "${header}"
            "${CMAKE_SOURCE_DIR}/pk/test/pk_test_moc.py"
        COMMENT "pk_test_moc ${header}"
        VERBATIM)
    set_source_files_properties("${source}" PROPERTIES OBJECT_DEPENDS "${binder}")
    set_target_properties(${target} PROPERTIES AUTOMOC OFF)
    target_compile_definitions(${target} PRIVATE
        KRITA_TESTSDK_PK_NATIVE
        PK_SHELL_MOC_BINDER)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

libkra_add_pk_test_binder(kis_kra_loader_test kis_kra_loader_test.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/kis_kra_loader_test.h")
libkra_add_pk_test_binder(kis_kra_saver_test kis_kra_saver_test.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/kis_kra_saver_test.h")
