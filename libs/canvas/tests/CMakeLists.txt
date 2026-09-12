kis_add_tests(
    KisAsyncColorSamplerHelperTest.cpp
    kis_aspect_ratio_locker_test.cpp
    kis_coordinates_converter_test.cpp
    kis_grid_config_test.cpp
    kis_multi_surface_policy_test.cpp
    kis_opengl_policy_test.cpp
    kis_prescaled_projection_test.cpp
    kis_selection_options_test.cpp
    kis_selection_tool_factory_test.cpp
    kis_stabilized_events_sampler_test.cpp
    zoomhandler_test.cpp
    LINK_LIBRARIES kritacanvas kritatestsdk
)

kis_add_test(
    fill_processing_visitor_test.cpp
    TEST_NAME FillProcessingVisitorTest
    NAME_PREFIX libs-canvas-
    LINK_LIBRARIES kritacanvas kritatestsdk pkimageio kritalcmsengine
)

find_program(GETTEXT_XGETTEXT_EXECUTABLE NAMES xgettext REQUIRED)
add_test(NAME libs-canvas-host-action-extraction
    COMMAND ${CMAKE_COMMAND}
        -DHOST_FACTORY_SOURCE=${CMAKE_SOURCE_DIR}/libs/flake/KoToolFactoryBase.cpp
        -DCANVAS_FACTORY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../tool/KisToolPaintFactoryBase.cpp
        -DXGETTEXT_EXECUTABLE=${GETTEXT_XGETTEXT_EXECUTABLE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_host_action_extraction.cmake)

add_test(NAME libs-canvas-selection-factory-host-boundary
    COMMAND ${CMAKE_COMMAND}
        -DSELECTION_FACTORY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/../tool/KisSelectionToolFactoryBase.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_selection_factory_host_boundary.cmake)

add_test(NAME libs-canvas-native-cursor-event-abi
    COMMAND ${CMAKE_COMMAND}
        -DCANVAS_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/..
        -DFLAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}/libs/flake
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_canvas_native_cursor_event_abi.cmake)


# R-65 T2+4 现场实测：kis_selection_tool_factory_test **不可迁**（保留在真 Qt 测试栈）。
# 实测命令（build-r65 首次 pk_add_test 注册后 ninja 产物）：
#   ninja -C build-r65 kis_selection_tool_factory_test
#   → FAILED: pk_binder_kis_selection_tool_factory_test.inc:9..13
#     error: static_cast from 'PkTestObject *' to 'KisSelectionToolFactoryTest *',
#            which are not related by inheritance, is not allowed
# 根因（预处理追索）：测试类写 `class KisSelectionToolFactoryTest : public QObject`
# （.cpp:53），而该 TU 第 13 行 `#include <QPointer>` 解析到
# pk/signal/compat/QPointer → `#include "QObject"`（**引号形式，先在它自己目录里找**）
# → pk/signal/compat/QObject → `#define QObject PkObject`，把 pk/test/compat/QObject
# 早先建立的 `QObject → PkTestObject` 覆盖掉（-Wno-macro-redefined 让这个覆盖无声）。
# 于是测试类派生成 PkObject，而 binder 的 static_cast<PkTestObject*> 与其无关。
# 反过来把 QObject 留住 PkTestObject 也不行：测试体第 176 行 `QObject collection;`
# 要传给 `KoToolFactoryBase::createActions(PkObject *actionCollection)`
# （libs/flake/KoToolFactoryBase.h:80）—— PkTestObject 不是 PkObject
# （pk/test/PkTestObject.h 的头注自己写着「R-05 交付真正的对象系统之后，compat/QObject
# 应改指过去」）。两侧互斥，根因在 pk/**（R-05 边界），不在本 Task 锁内。
