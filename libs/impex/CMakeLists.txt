set(kritaimpex_LIB_SRCS
    KisExportCheckBase.cpp
    KisPreExportChecker.cpp
    KisExportCheckRegistry.cpp
    KisImportExportBackend.cpp
    KisImportExportFilter.cpp
    KisImportExportManager.cpp
    KisPkImageFileDecoderAdapter.cpp
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

# Format consumers use the migrated Pk-facing portions of impex headers.  Keep
# the real DSO as a link-only edge so its legacy Qt usage requirements do not
# become compile requirements of those consumers.
add_library(kritaimpex_noqt_compile_interface INTERFACE)
target_include_directories(kritaimpex_noqt_compile_interface INTERFACE
    ${CMAKE_SOURCE_DIR}/libs/impex
    ${CMAKE_BINARY_DIR}/libs/impex
    ${CMAKE_SOURCE_DIR}/libs/image
    ${CMAKE_BINARY_DIR}/libs/image
    ${CMAKE_SOURCE_DIR}/libs/metadata
    ${CMAKE_BINARY_DIR}/libs/metadata
    ${CMAKE_SOURCE_DIR}/libs/psdutils
    ${CMAKE_BINARY_DIR}/libs/psdutils
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/port
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/global
    ${CMAKE_SOURCE_DIR}/pk/color
    ${CMAKE_SOURCE_DIR}/pk/xml
    ${CMAKE_SOURCE_DIR}/pk/log
    ${CMAKE_SOURCE_DIR}/pk/uuid
)
target_link_libraries(kritaimpex_noqt_compile_interface INTERFACE
    kritaimage_noqt_compile_interface
    pkstring pkcontainer pkvariant pkport pkpointer pkgeometry pkglobal
    pkcolor pkxml pklog pkuuid
)

# KisPkImageFileDecoderAdapter.cpp includes these Pk headers directly. Keep
# the adapter's private closure explicit instead of relying on transitive
# include directories exported by Krita libraries. pkimageio remains the
# single shared process-wide decoder registry/provider.
target_include_directories(kritaimpex PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/image
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/namespace
    ${CMAKE_SOURCE_DIR}/pk/variant
    ${CMAKE_SOURCE_DIR}/pk/pointer
    ${CMAKE_SOURCE_DIR}/pk/string
    ${CMAKE_SOURCE_DIR}/pk/log
)

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
        pkimageio
        pkimage
        pkcontainer
        pkgeometry
        pknamespace
        pkvariant
        pkpointer
        pkstring
        pklog
)

if(UNIX AND NOT APPLE)
    # The process-wide decoder registry retains callbacks into this DSO.
    target_link_options(kritaimpex PRIVATE -Wl,-z,nodelete)
endif()

set_target_properties(kritaimpex
    PROPERTIES VERSION ${GENERIC_KRITA_LIB_VERSION} SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritaimpex  ${INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
