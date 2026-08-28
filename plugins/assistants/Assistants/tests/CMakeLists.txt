kis_add_tests(
    TestPerspectiveBasedAssistantHelper.cpp
    TestAssistants.cpp
    LINK_LIBRARIES kritaassistanttool_static
    )
set_target_properties(TestPerspectiveBasedAssistantHelper TestAssistants PROPERTIES AUTOMOC OFF)

kis_add_test(
    TestPaintingAssistantModel.cpp
    TEST_NAME TestPaintingAssistantModel
    LINK_LIBRARIES kritaassistanttool_static
)
set_target_properties(TestPaintingAssistantModel PROPERTIES AUTOMOC OFF)

kis_add_test(
    AssistantCoreTest.cpp
    ../kis_abstract_perspective_grid.cpp
    ../kis_painting_assistant.cpp
    ../kis_painting_assistant_collection.cpp
    ../kis_painting_assistant_handle.cc
    ../kis_painting_assistant_registry.cc
    ../ConcentricEllipseAssistantGeometry.cpp
    ../Ellipse.cc
    ../PerspectiveBasedAssistantHelper.cpp
    ../Ruler.cc
    ../assistant_tool.cc
    ../ConcentricEllipseAssistant.cc
    ../CurvilinearPerspectiveAssistant.cc
    ../EllipseAssistant.cc
    ../FisheyePointAssistant.cc
    ../InfiniteRulerAssistant.cc
    ../ParallelRulerAssistant.cc
    ../PerspectiveAssistant.cc
    ../PerspectiveEllipseAssistant.cc
    ../RulerAssistant.cc
    ../SplineAssistant.cc
    ../TwoPointAssistant.cc
    ../VanishingPointAssistant.cc
    TEST_NAME AssistantCoreTest
    LINK_LIBRARIES Boost::boost Eigen3::Eigen kritaglobal pkcolor
)
set_target_properties(AssistantCoreTest PROPERTIES AUTOMOC OFF)
target_include_directories(AssistantCoreTest BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_BINARY_DIR}/..
    ${CMAKE_SOURCE_DIR}/libs/global
    ${CMAKE_BINARY_DIR}/libs/global
    ${CMAKE_SOURCE_DIR}/libs/image
)
