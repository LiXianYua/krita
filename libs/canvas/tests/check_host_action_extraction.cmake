if(NOT DEFINED HOST_FACTORY_SOURCE OR NOT DEFINED CANVAS_FACTORY_SOURCE)
    message(FATAL_ERROR "Host and Canvas source paths are required")
endif()

file(READ "${HOST_FACTORY_SOURCE}" host_source)
file(READ "${CANVAS_FACTORY_SOURCE}" canvas_source)

set(labels
    "Increase Brush Size"
    "Decrease Brush Size"
    "Rotate brush tip clockwise"
    "Rotate brush tip clockwise (precise)"
    "Rotate brush tip counter-clockwise"
    "Rotate brush tip counter-clockwise (precise)")

foreach(label IN LISTS labels)
    string(FIND "${host_source}" "i18n(\"${label}\")" host_label_offset)
    if(host_label_offset EQUAL -1)
        message(FATAL_ERROR "Host action label is not an extractable i18n literal: ${label}")
    endif()

    string(FIND "${canvas_source}" "\"${label}\"" canvas_label_offset)
    if(canvas_label_offset EQUAL -1)
        message(FATAL_ERROR "Canvas action label is missing: ${label}")
    endif()
endforeach()

message(STATUS "Verified six host-action labels have matching extractable i18n literals")
