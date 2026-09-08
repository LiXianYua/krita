execute_process(COMMAND "${ORACLE}" "${OUTPUT_DIR}/svg-qt.bin"
    OUTPUT_FILE "${OUTPUT_DIR}/svg-qt.tsv" RESULT_VARIABLE oracle_result)
execute_process(COMMAND "${NATIVE}" "${OUTPUT_DIR}/svg-native.bin"
    OUTPUT_FILE "${OUTPUT_DIR}/svg-native.tsv" RESULT_VARIABLE native_result)
if(NOT oracle_result EQUAL 0 OR NOT native_result EQUAL 0)
    message(FATAL_ERROR "SVG renderer failed: Qt=${oracle_result}, native=${native_result}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${OUTPUT_DIR}/svg-qt.tsv" "${OUTPUT_DIR}/svg-native.tsv" RESULT_VARIABLE metrics_result)
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${OUTPUT_DIR}/svg-qt.bin" "${OUTPUT_DIR}/svg-native.bin" RESULT_VARIABLE pixels_result)
if(NOT metrics_result EQUAL 0 OR NOT pixels_result EQUAL 0)
    message(FATAL_ERROR "SVG Qt/native dimensions or raw pixels differ; see ${OUTPUT_DIR}/svg-{qt,native}.{tsv,bin}")
endif()
