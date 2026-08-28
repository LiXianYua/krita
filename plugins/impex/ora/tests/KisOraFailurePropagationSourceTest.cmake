file(READ "${ORA_DIR}/kis_open_raster_save_context.cpp" CONTEXT_SOURCE)
file(READ "${ORA_DIR}/kis_open_raster_stack_save_visitor.cpp" VISITOR_SOURCE)
file(READ "${ORA_DIR}/ora_converter.cpp" CONVERTER_SOURCE)

foreach(REQUIRED IN ITEMS
        "if (!m_store->open(\"stack.xml\"))"
        "oraWriteAll("
        "return writeSucceeded && closeSucceeded;")
    string(FIND "${CONTEXT_SOURCE}" "${REQUIRED}" MATCH_INDEX)
    if(MATCH_INDEX EQUAL -1)
        message(FATAL_ERROR "ORA save context is missing failure propagation token: ${REQUIRED}")
    endif()
endforeach()

foreach(REQUIRED IN ITEMS
        "oraLayerPayloadSucceeded(filename)"
        "!d->saveContext->saveStack(d->layerStack)"
        "visitAll(layer, true)")
    string(FIND "${VISITOR_SOURCE}" "${REQUIRED}" MATCH_INDEX)
    if(MATCH_INDEX EQUAL -1)
        message(FATAL_ERROR "ORA visitor is missing failure propagation token: ${REQUIRED}")
    endif()
endforeach()

foreach(REQUIRED IN ITEMS
        "!image->rootLayer()->accept(orssv)"
        "return ImportExportCodes::ErrorWhileWriting;")
    string(FIND "${CONVERTER_SOURCE}" "${REQUIRED}" MATCH_INDEX)
    if(MATCH_INDEX EQUAL -1)
        message(FATAL_ERROR "ORA converter is missing failure propagation token: ${REQUIRED}")
    endif()
endforeach()
