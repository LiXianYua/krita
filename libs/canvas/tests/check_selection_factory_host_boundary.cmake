if(NOT DEFINED SELECTION_FACTORY_SOURCE)
    message(FATAL_ERROR "Selection factory source path is required")
endif()

file(READ "${SELECTION_FACTORY_SOURCE}" source)
string(FIND "${source}" "setObjectName" set_name_offset)
if(NOT set_name_offset EQUAL -1)
    message(FATAL_ERROR "Selection factory must delegate action naming to the host factory")
endif()

string(FIND "${source}" "new QAction" new_action_offset)
if(NOT new_action_offset EQUAL -1)
    message(FATAL_ERROR "Selection factory must delegate action creation to the host factory")
endif()

foreach(name IN ITEMS
        selection_tool_mode_add
        selection_tool_mode_replace
        selection_tool_mode_subtract
        selection_tool_mode_intersect
        undo_polygon_selection)
    string(FIND "${source}" "createHostAction(\"\", \"${name}\")" name_offset)
    if(name_offset EQUAL -1)
        message(FATAL_ERROR "Host action is missing or not named through createHostAction: ${name}")
    endif()
endforeach()

message(STATUS "Verified selection and polyline action creation stays at the host boundary")
