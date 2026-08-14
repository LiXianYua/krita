set(kritaimpex_LIB_SRCS
    KisExportCheckBase.cpp
    KisPreExportChecker.cpp
    KisExportCheckRegistry.cpp
    KisImportExportBackend.cpp
    KisImportExportFilter.cpp
    KisImportExportManager.cpp
    KisImportExportUtils.cpp
    KisImportExportErrorCode.cpp
    KisImportExportAdditionalChecks.cpp
    KisImportUserFeedbackInterface.cpp
    KisDocumentApplicationServices.cpp
    KisDocument.cpp
    KisCloneDocumentStroke.cpp
    KisDecorationsWrapperLayer.cpp
    kis_document_undo_store.cpp
    StoryboardItem.cpp
    KisDocumentRegistry.cpp
    KisDocumentRegistry.h
    KisReferenceImageDocumentFallback.cpp
    KisReferenceImagesLayerCommands.cpp
    KisPngCodec.cpp
    KoDocumentInfo.cpp
)

kis_add_library(kritaimpex SHARED ${kritaimpex_HEADERS_MOC} ${kritaimpex_LIB_SRCS})

generate_export_header(kritaimpex BASE_NAME kritaimpex)

target_link_libraries(kritaimpex
    PUBLIC
        KF${KF_MAJOR}::CoreAddons
        KF${KF_MAJOR}::I18n
        Qt${QT_MAJOR_VERSION}::Concurrent
        kritaimage
        kritashapemodel
    PRIVATE
        KF${KF_MAJOR}::ConfigCore
        ${PNG_LIBRARIES}
        kritaassistanttool_static
        kritacanvas
        kritaresources
        kritastore
)

set_target_properties(kritaimpex
    PROPERTIES VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritaimpex  ${INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
