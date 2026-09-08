set(PSD_TEST_LIBS kritapsdutils kritatestsdk)

kis_add_tests(
    psd_utils_test.cpp
    compression_test.cpp
    psd_cos_parser_test.cpp
    LINK_LIBRARIES ${PSD_TEST_LIBS})

function(psdutils_add_pk_test_binder target source header)
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

psdutils_add_pk_test_binder(psd_utils_test psd_utils_test.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/psd_utils_test.h")
psdutils_add_pk_test_binder(psd_cos_parser_test psd_cos_parser_test.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/psd_cos_parser_test.h")
