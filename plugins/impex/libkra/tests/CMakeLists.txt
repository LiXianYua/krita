kis_add_tests(
    kis_kra_loader_test.cpp
    LINK_LIBRARIES
        kritaimpex kritalibkra kritatestsdk
        kritakraimport kritakraexport kritalcmsengine
        -Wl,--whole-archive kritaexample -Wl,--no-whole-archive
    )

kis_add_test(
    kis_file_layer_roundtrip_test.cpp
    LINK_LIBRARIES kritaimpex kritalibkra kritatestsdk
    )


kis_add_test(
    kis_kra_saver_test.cpp
    LINK_LIBRARIES
        kritaimpex kritalibkra kritatransformmaskstubs
        kritakraimport kritakraexport kritalcmsengine
        -Wl,--whole-archive
            kritaexample kritapixelizefilter
            kritacolorgenerator kritapatterngenerator
        -Wl,--no-whole-archive
    )

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
