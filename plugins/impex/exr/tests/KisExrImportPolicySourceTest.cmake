file(READ "${EXR_DIR}/exr_converter.cc" CONVERTER_SOURCE)
file(READ "${EXR_DIR}/kis_exr_layers_sorter.cpp" SORTER_SOURCE)

foreach(REQUIRED IN ITEMS
        "preferredExrColorProfile(defaultProfileForColorSpace)"
        "hasUsableExrLayersMetadata(hasExtraLayersAttribute, extraLayersInfo)"
        "if (useExtraLayersInfo)"
        "classifyExrChannels(channelKeys)")
    string(FIND "${CONVERTER_SOURCE}" "${REQUIRED}" MATCH_INDEX)
    if(MATCH_INDEX EQUAL -1)
        message(FATAL_ERROR "EXR production importer is missing policy call: ${REQUIRED}")
    endif()
endforeach()

string(FIND "${CONVERTER_SOURCE}" "extraLayersInfo.isNull()" NULL_SENTINEL_INDEX)
if(NOT NULL_SENTINEL_INDEX EQUAL -1)
    message(FATAL_ERROR "EXR production importer still uses PkXmlDocument::isNull as metadata sentinel")
endif()

string(FIND "${SORTER_SOURCE}" "extraData.documentElement().isNull()" ELEMENT_GATE_INDEX)
if(ELEMENT_GATE_INDEX EQUAL -1)
    message(FATAL_ERROR "EXR layer sorter does not validate the document element")
endif()
