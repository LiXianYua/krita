execute_process(COMMAND "${ORACLE}" --pixels "${OUTPUT_DIR}/qt-pixels.bin" RESULT_VARIABLE oracle_status
    OUTPUT_FILE "${OUTPUT_DIR}/qt-pixels.tsv" ERROR_FILE "${OUTPUT_DIR}/qt-stderr.log")
execute_process(COMMAND "${NATIVE}" --pixels "${OUTPUT_DIR}/native-pixels.bin" RESULT_VARIABLE native_status
    OUTPUT_FILE "${OUTPUT_DIR}/native-pixels.tsv" ERROR_FILE "${OUTPUT_DIR}/native-stderr.log")
if(NOT oracle_status EQUAL 0 OR NOT native_status EQUAL 0)
    message(FATAL_ERROR "Font oracle failed: Qt=${oracle_status}; native=${native_status}")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${OUTPUT_DIR}/qt-pixels.bin" "${OUTPUT_DIR}/native-pixels.bin"
    RESULT_VARIABLE raw_status)
if(NOT raw_status EQUAL 0)
    message(FATAL_ERROR "Font raw pixels differ: compare qt-pixels.bin with native-pixels.bin")
endif()
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
    "${OUTPUT_DIR}/qt-pixels.tsv" "${OUTPUT_DIR}/native-pixels.tsv"
    RESULT_VARIABLE comparison_status)
if(NOT comparison_status EQUAL 0)
    message(FATAL_ERROR "Font pixels differ: compare qt-pixels.tsv with native-pixels.tsv")
endif()
