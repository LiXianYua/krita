if(NOT DEFINED HOST_FACTORY_SOURCE OR
   NOT DEFINED CANVAS_FACTORY_SOURCE OR
   NOT DEFINED XGETTEXT_EXECUTABLE OR
   XGETTEXT_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "Host source, Canvas source, and xgettext are required")
endif()

file(READ "${CANVAS_FACTORY_SOURCE}" canvas_source)

string(RANDOM LENGTH 12 pot_suffix)
set(pot_file "${CMAKE_CURRENT_BINARY_DIR}/host-action-${pot_suffix}.pot")
execute_process(
    COMMAND "${XGETTEXT_EXECUTABLE}"
        --language=C++
        --from-code=UTF-8
        --keyword=i18n:1
        --output=${pot_file}
        "${HOST_FACTORY_SOURCE}"
    RESULT_VARIABLE xgettext_result
    ERROR_VARIABLE xgettext_error)
if(NOT xgettext_result EQUAL 0)
    file(REMOVE "${pot_file}")
    message(FATAL_ERROR "xgettext failed (${xgettext_result}): ${xgettext_error}")
endif()
file(READ "${pot_file}" extracted_messages)
file(REMOVE "${pot_file}")

set(labels
    "Increase Brush Size"
    "Decrease Brush Size"
    "Rotate brush tip clockwise"
    "Rotate brush tip clockwise (precise)"
    "Rotate brush tip counter-clockwise"
    "Rotate brush tip counter-clockwise (precise)")

foreach(label IN LISTS labels)
    string(FIND "${extracted_messages}" "msgid \"${label}\"" extracted_label_offset)
    if(extracted_label_offset EQUAL -1)
        message(FATAL_ERROR "xgettext did not extract the contextless host-action msgid: ${label}")
    endif()

    string(FIND "${canvas_source}" "\"${label}\"" canvas_label_offset)
    if(canvas_label_offset EQUAL -1)
        message(FATAL_ERROR "Canvas action label is missing: ${label}")
    endif()
endforeach()

message(STATUS "Verified xgettext emits six contextless host-action msgids matching Canvas")
