file(READ "${SVG_SOURCE}" SVG_CONTENT)

foreach(FORBIDDEN IN ITEMS QBuffer QByteArray QString QRectF QSizeF QStringList createShapesFromSvg)
    string(FIND "${SVG_CONTENT}" "${FORBIDDEN}" MATCH_INDEX)
    if(NOT MATCH_INDEX EQUAL -1)
        message(FATAL_ERROR "SVG production importer still contains forbidden Qt/downstream token: ${FORBIDDEN}")
    endif()
endforeach()

string(FIND "${SVG_CONTENT}" "return ImportExportCodes::FormatFeaturesUnsupported;" UNSUPPORTED_INDEX)
if(UNSUPPORTED_INDEX EQUAL -1)
    message(FATAL_ERROR "SVG production importer does not return truthful FormatFeaturesUnsupported GAP")
endif()
