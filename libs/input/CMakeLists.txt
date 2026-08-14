set(kritainput_LIB_SRCS
    KisInputActionGroup.cpp
    KisInputConfig.cpp
    KisInputTransformPolicy.cpp
    kis_abstract_input_action.cpp
    kis_abstract_shortcut.cpp
    kis_native_gesture_shortcut.cpp
    kis_single_action_shortcut.cpp
    kis_stroke_shortcut.cpp
    kis_touch_shortcut.cpp
    kis_shortcut_configuration.cpp
    kis_shortcut_matcher.cpp
)

kis_add_library(kritainput SHARED ${kritainput_LIB_SRCS})
generate_export_header(kritainput BASE_NAME kritainput)

target_include_directories(kritainput
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(kritainput
    PUBLIC
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Gui
    PRIVATE
        KF${KF_MAJOR}::ConfigCore
        KF${KF_MAJOR}::I18n
        kritaflake
        kritaglobal
)

set_target_properties(kritainput PROPERTIES
    VERSION ${GENERIC_KRITA_LIB_VERSION}
    SOVERSION ${GENERIC_KRITA_LIB_SOVERSION}
)

install(TARGETS kritainput ${INSTALL_TARGETS_DEFAULT_ARGS})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
