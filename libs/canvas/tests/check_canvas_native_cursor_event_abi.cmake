if(EXISTS "${CANVAS_SOURCE_DIR}/noqt-compat/KoPointerEvent.h")
    message(FATAL_ERROR "canvas must not shadow the canonical KoPointerEvent declaration")
endif()

if(EXISTS "${CANVAS_SOURCE_DIR}/noqt-compat/PkFlakeBridge.h")
    message(FATAL_ERROR "canvas must not interpose PkFlakeBridge with include_next")
endif()

file(READ "${FLAKE_SOURCE_DIR}/KoPointerEvent.h" pointer_event_header)
if(pointer_event_header MATCHES "#include[ \t]*<Qt(Core|Gui)/")
    message(FATAL_ERROR "canonical KoPointerEvent public ABI still imports a Qt umbrella")
endif()
foreach(native_type
        "Pk::MouseButton button() const"
        "Pk::MouseButtons buttons() const"
        "Pk::KeyboardModifiers modifiers() const")
    string(FIND "${pointer_event_header}" "${native_type}" native_type_offset)
    if(native_type_offset EQUAL -1)
        message(FATAL_ERROR "canonical KoPointerEvent is missing native API: ${native_type}")
    endif()
endforeach()

file(READ "${FLAKE_SOURCE_DIR}/KoCanvasCursorHost.h" cursor_host_header)
foreach(contract_phrase
        "immutable snapshot"
        "owning UI thread"
        "scoped to one"
        "never reused"
        "zero"
        "default platform cursor")
    string(FIND "${cursor_host_header}" "${contract_phrase}" contract_phrase_offset)
    if(contract_phrase_offset EQUAL -1)
        message(FATAL_ERROR "cursor token contract is missing: ${contract_phrase}")
    endif()
endforeach()
