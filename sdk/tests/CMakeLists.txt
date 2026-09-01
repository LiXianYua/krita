add_library(kritatestsdk INTERFACE)

target_link_libraries(kritatestsdk INTERFACE
    Qt${QT_MAJOR_VERSION}::Test
    kritaglobal
    pktest
)

target_compile_definitions(kritatestsdk INTERFACE 
    FILES_DEFAULT_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
    SYSTEM_RESOURCES_DATA_DIR="${CMAKE_SOURCE_DIR}/data/"
)

if(BUILD_TESTING)
    # The standalone S-03-b shell maps QObject to PkTestObject and generates
    # this binder in its own binary directory.  The transitional main tree
    # keeps TestKUndo2Stack as a real QObject and dispatches it through QTest,
    # so its historical same-TU binder include must resolve to a no-op here.
    file(GENERATE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pk_binder_testkundo2stack.inc"
        CONTENT "// Main-tree QObject branch: QTest discovers slots through moc.\n"
    )

    kis_add_test(
        PkSimpleTestBridgeTest.cpp
        LINK_LIBRARIES kritatestsdk pkconcurrent
    )
    target_compile_definitions(PkSimpleTestBridgeTest PRIVATE KRITA_TESTSDK_PK_NATIVE)
endif()

add_library(kritatransformmaskstubs STATIC
    KisDumbTransformMaskParams.cpp
    KisDumbAnimatedTransformMaskParamsHolder.cpp
    KritaTransformMaskStubs.cpp
)

generate_export_header(kritatransformmaskstubs BASE_NAME kritatransformmaskstubs)

target_link_libraries(kritatransformmaskstubs INTERFACE kritatestsdk)
target_link_libraries(kritatransformmaskstubs PUBLIC kritaimage)
