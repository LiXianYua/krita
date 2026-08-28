if(NOT BUILD_DIR OR NOT INSTALL_PREFIX OR NOT REGISTRAR_NAME OR NOT CONSUMER_NAME)
    message(FATAL_ERROR "missing install verification arguments")
endif()
if(NOT CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif()
if(NOT CMAKE_INSTALL_BINDIR)
    set(CMAKE_INSTALL_BINDIR bin)
endif()

file(REMOVE_RECURSE "${INSTALL_PREFIX}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "install failed (${install_result})\n${install_output}\n${install_error}")
endif()

set(lib_dir "${INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
if(NOT EXISTS "${lib_dir}/libpkimageio.so")
    message(FATAL_ERROR "installed provider missing: ${lib_dir}/libpkimageio.so")
endif()
set(registrar "${lib_dir}/${REGISTRAR_NAME}")
set(consumer "${INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${CONSUMER_NAME}")
if(NOT EXISTS "${registrar}" OR NOT EXISTS "${consumer}")
    message(FATAL_ERROR "installed registrar/consumer missing")
endif()

set(loader_path "${lib_dir}:$ENV{LD_LIBRARY_PATH}")
if(CI_LIBRARY_DIR)
    set(loader_path "${lib_dir}:${CI_LIBRARY_DIR}:$ENV{LD_LIBRARY_PATH}")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "LD_LIBRARY_PATH=${loader_path}"
            "${consumer}" "${registrar}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error)
if(NOT run_result EQUAL 0)
    message(FATAL_ERROR "installed DSO decode failed (${run_result})\n${run_output}\n${run_error}")
endif()

foreach(binary IN ITEMS "${consumer}" "${registrar}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env "LD_LIBRARY_PATH=${loader_path}" "ldd" "${binary}"
        RESULT_VARIABLE ldd_result
        OUTPUT_VARIABLE ldd_output
        ERROR_VARIABLE ldd_error)
    if(NOT ldd_result EQUAL 0 OR NOT ldd_output MATCHES "${lib_dir}/libpkimageio\\.so")
        message(FATAL_ERROR "${binary} does not resolve installed provider\n${ldd_output}\n${ldd_error}")
    endif()
endforeach()
message(STATUS "installed provider: ${lib_dir}/libpkimageio.so")
message(STATUS "installed registrar and consumer resolve the same provider")
