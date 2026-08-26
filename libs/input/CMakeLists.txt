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
        # S-08-a Task 7（判据⑧）：pk include 面 PUBLIC 传播给消费方。只加 pk/<dir>
        # 不加 pk/*/compat——主树保留过渡 Qt，真实 Qt 头（QString/QtGlobal 等）不能被遮蔽。
        # 覆盖剥-ed 源 include 的 PkString/PkList/PkVector/PkPoint/PkLine/PkFlags/
        # PkSharedPointer/PkConfigGroup/PkSharedConfig/PkGlobal/PkNamespace。
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/string>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/container>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/geometry>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/flags>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/pointer>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/config>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/global>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/pk/namespace>
)

target_link_libraries(kritainput
    PUBLIC
        Qt${QT_MAJOR_VERSION}::Core
        Qt${QT_MAJOR_VERSION}::Gui
        # S-08-a Task 7（判据⑨）：剥-ed 源引用的 pk 库显式链入主树 kritainput。
        # pk 库 target 由根 CMakeLists.txt:1642 的 add_subdirectory(pk) 定义，此处只接线。
        # pkflags/pknamespace 是 INTERFACE 头库，无需 link。
        pkstring pkcontainer pkgeometry pkpointer pkconfig pkglobal
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
