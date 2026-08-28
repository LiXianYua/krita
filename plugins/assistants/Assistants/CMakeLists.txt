add_subdirectory(tests)

set(kritaassistanttool_GEOMETRY_SOURCES
    kis_abstract_perspective_grid.cpp
    kis_painting_assistant.cpp
    kis_painting_assistant_collection.cpp
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

kis_add_library(kritaassistanttool_static STATIC
    ${kritaassistanttool_GEOMETRY_SOURCES}
    ${kritaassistanttool_UI_SOURCES}
)

generate_export_header(kritaassistanttool_static BASE_NAME kritaassistanttool EXPORT_MACRO_NAME KRITAASSISTANTTOOL_EXPORT)

target_include_directories(kritaassistanttool_static
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

target_link_libraries(kritaassistanttool_static
    PUBLIC
        Boost::boost
        kritaglobal
        kritastore
        pkcolor
    PRIVATE
        Eigen3::Eigen
)
set_target_properties(kritaassistanttool_static PROPERTIES AUTOMOC OFF)

kis_add_library(kritaassistanttool MODULE assistant_tool.cc)
target_link_libraries(kritaassistanttool PRIVATE kritaassistanttool_static)
set_target_properties(kritaassistanttool PROPERTIES AUTOMOC OFF)

install(TARGETS kritaassistanttool  DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
