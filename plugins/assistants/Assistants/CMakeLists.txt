add_subdirectory(tests)

set(kritaassistanttool_GEOMETRY_SOURCES
    kis_abstract_perspective_grid.cpp
    kis_painting_assistant.cpp
    kis_painting_assistant_handle.cc
    kis_painting_assistant_registry.cc
    ConcentricEllipseAssistantGeometry.cpp
    Ellipse.cc
    PerspectiveBasedAssistantHelper.cpp
    Ruler.cc
)

set(kritaassistanttool_UI_SOURCES
    ConcentricEllipseAssistant.cc
    CurvilinearPerspectiveAssistant.cc
    EllipseAssistant.cc
    FisheyePointAssistant.cc
    InfiniteRulerAssistant.cc
    ParallelRulerAssistant.cc
    PerspectiveAssistant.cc
    RulerAssistant.cc
    SplineAssistant.cc
    VanishingPointAssistant.cc
    TwoPointAssistant.cc
    PerspectiveEllipseAssistant.cc
)

kis_add_library(kritaassistanttool_static STATIC ${kritaassistanttool_GEOMETRY_SOURCES})

generate_export_header(kritaassistanttool_static BASE_NAME kritaassistanttool EXPORT_MACRO_NAME KRITAASSISTANTTOOL_EXPORT)

target_include_directories(kritaassistanttool_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(kritaassistanttool_static
    PUBLIC
        kritaflake
        kritaimage
    PRIVATE
        kritacanvas
        KF${KF_MAJOR}::ConfigCore
)

kis_add_library(kritaassistanttool MODULE assistant_tool.cc ${kritaassistanttool_UI_SOURCES})
target_link_libraries(kritaassistanttool kritaassistanttool_static kritacanvas)

install(TARGETS kritaassistanttool  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
