add_subdirectory(tests)

set(kritaassistanttool_SOURCES
    kis_painting_assistant.cc
    ConcentricEllipseAssistant.cc
    CurvilinearPerspectiveAssistant.cc
    Ellipse.cc
    EllipseAssistant.cc
    FisheyePointAssistant.cc
    InfiniteRulerAssistant.cc
    ParallelRulerAssistant.cc
    PerspectiveBasedAssistantHelper.cpp
    PerspectiveAssistant.cc
    Ruler.cc
    RulerAssistant.cc
    SplineAssistant.cc
    VanishingPointAssistant.cc
    TwoPointAssistant.cc
    PerspectiveEllipseAssistant.cc
)

kis_add_library(kritaassistanttool_static STATIC ${kritaassistanttool_SOURCES})

generate_export_header(kritaassistanttool_static BASE_NAME kritaassistanttool EXPORT_MACRO_NAME KRITAASSISTANTTOOL_EXPORT)

target_include_directories(kritaassistanttool_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/ui
        ${CMAKE_SOURCE_DIR}/libs/ui/canvas
        ${CMAKE_BINARY_DIR}/libs/ui
)

target_link_libraries(kritaassistanttool_static kritaflake kritaimage kritawidgets)

kis_add_library(kritaassistanttool MODULE assistant_tool.cc EditAssistantsCommand.cpp)
target_link_libraries(kritaassistanttool kritaassistanttool_static kritaui)

install(TARGETS kritaassistanttool  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
