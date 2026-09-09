foreach(required_variable IN ITEMS
        KARBON_RESOURCE_SOURCE_DIR
        KARBON_RESOURCE_FIXTURE_SOURCE_DIR
        KARBON_RESOURCE_FIXTURE_BINARY_DIR
        KARBON_RESOURCE_TEST_GENERATOR)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

set(fixture_source "${KARBON_RESOURCE_FIXTURE_BINARY_DIR}/source")
set(fixture_build "${KARBON_RESOURCE_FIXTURE_BINARY_DIR}/build")
file(REMOVE_RECURSE "${KARBON_RESOURCE_FIXTURE_BINARY_DIR}")
file(COPY "${KARBON_RESOURCE_FIXTURE_SOURCE_DIR}/" DESTINATION "${fixture_source}")
configure_file(
    "${KARBON_RESOURCE_SOURCE_DIR}/22-actions-calligraphy.png"
    "${fixture_source}/calligraphy.png"
    COPYONLY)

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${fixture_source}" -B "${fixture_build}"
        -G "${KARBON_RESOURCE_TEST_GENERATOR}"
        -DKARBON_RESOURCE_SOURCE_DIR=${KARBON_RESOURCE_SOURCE_DIR}
        -DCMAKE_C_COMPILER_LAUNCHER=ccache
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
    TIMEOUT 60)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "fixture configure failed (${configure_result})\n${configure_output}\n${configure_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${fixture_build}" --target karbon_resource_probe
    RESULT_VARIABLE first_build_result
    OUTPUT_VARIABLE first_build_output
    ERROR_VARIABLE first_build_error
    TIMEOUT 60)
if(NOT first_build_result EQUAL 0)
    message(FATAL_ERROR
        "initial fixture build failed (${first_build_result})\n${first_build_output}\n${first_build_error}")
endif()
execute_process(
    COMMAND "${fixture_build}/karbon_resource_probe"
    RESULT_VARIABLE first_probe_result
    OUTPUT_VARIABLE first_probe_output
    ERROR_VARIABLE first_probe_error
    TIMEOUT 10)
if(NOT first_probe_result EQUAL 0)
    message(FATAL_ERROR
        "initial fixture probe failed (${first_probe_result})\n${first_probe_error}")
endif()

# Change only the fixture's PNG input.  The subsequent build must regenerate the
# configured member, recompile it, and expose different embedded bytes.
file(APPEND "${fixture_source}/calligraphy.png" "incremental-change")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${fixture_build}" --target karbon_resource_probe
    RESULT_VARIABLE second_build_result
    OUTPUT_VARIABLE second_build_output
    ERROR_VARIABLE second_build_error
    TIMEOUT 60)
if(NOT second_build_result EQUAL 0)
    message(FATAL_ERROR
        "incremental fixture build failed (${second_build_result})\n${second_build_output}\n${second_build_error}")
endif()
execute_process(
    COMMAND "${fixture_build}/karbon_resource_probe"
    RESULT_VARIABLE second_probe_result
    OUTPUT_VARIABLE second_probe_output
    ERROR_VARIABLE second_probe_error
    TIMEOUT 10)
if(NOT second_probe_result EQUAL 0)
    message(FATAL_ERROR
        "incremental fixture probe failed (${second_probe_result})\n${second_probe_error}")
endif()
if(first_probe_output STREQUAL second_probe_output)
    message(FATAL_ERROR
        "changing only the PNG left the embedded probe output unchanged: ${first_probe_output}")
endif()

# Leave the isolated fixture on the canonical input for repeatable inspection.
configure_file(
    "${KARBON_RESOURCE_SOURCE_DIR}/22-actions-calligraphy.png"
    "${fixture_source}/calligraphy.png"
    COPYONLY)
execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${fixture_build}" --target karbon_resource_probe
    RESULT_VARIABLE restore_build_result
    OUTPUT_VARIABLE restore_build_output
    ERROR_VARIABLE restore_build_error
    TIMEOUT 60)
if(NOT restore_build_result EQUAL 0)
    message(FATAL_ERROR
        "canonical restore build failed (${restore_build_result})\n${restore_build_output}\n${restore_build_error}")
endif()
execute_process(
    COMMAND "${fixture_build}/karbon_resource_probe"
    RESULT_VARIABLE restore_probe_result
    OUTPUT_VARIABLE restore_probe_output
    ERROR_VARIABLE restore_probe_error
    TIMEOUT 10)
if(NOT restore_probe_result EQUAL 0 OR NOT restore_probe_output STREQUAL first_probe_output)
    message(FATAL_ERROR
        "canonical restore did not reproduce the original embedded output\n"
        "initial=${first_probe_output} restored=${restore_probe_output} error=${restore_probe_error}")
endif()
message(STATUS
    "PNG-only rebuild changed embedded output: ${first_probe_output} -> ${second_probe_output}; "
    "canonical restore returned ${restore_probe_output}")
