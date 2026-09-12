project(SvgTextTool)
add_subdirectory(tests)

set (SvgTextTool_SRCS
    SvgCreateTextStrategy.cpp
    SvgInlineSizeChangeCommand.cpp
    SvgInlineSizeChangeStrategy.cpp
    SvgSelectTextStrategy.cpp
    SvgMoveTextCommand.cpp
    SvgMoveTextStrategy.cpp
    SvgTextChangeCommand.cpp
    SvgTextInsertCommand.cpp
    SvgTextInsertRichCommand.cpp
    SvgTextRemoveCommand.cpp
    SvgTextMergePropertiesRangeCommand.cpp
    SvgTextCursor.cpp
    SvgTextTool.cpp
    SvgTextToolFactory.cpp
    SvgTextShapeManagerBlocker.cpp
    SvgTextShortCuts.cpp
    SvgTextToolOptionsData.cpp
    SvgTextChangeTransformsOnRange.cpp
    SvgTextTypeSettingStrategy.cpp
    SvgTextRemoveTransformsFromRange.cpp
    SvgChangeTextPathInfoStrategy.cpp
    SvgTextOnPathDecorationHelper.cpp
    SvgChangeTextPaddingMarginStrategy.cpp
)

install( FILES
    SvgTextTool.action
    DESTINATION  ${KDE_INSTALL_DATADIR}/krita/actions
)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/svgtexttool.xmlgui" SVGTEXTTOOL_XMLGUI)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/svgtexttool.xmlgui")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/qrc_svgtexttool.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/qrc_svgtexttool.cpp"
    @ONLY)

set(SvgTextTool_SOURCES
    Plugin.cpp
    SvgTextQtPlatformHost.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/qrc_svgtexttool.cpp"
)


kis_add_library(krita_tool_svgtext_static STATIC ${SvgTextTool_SRCS})
kis_add_library(krita_tool_svgtext STATIC ${SvgTextTool_SOURCES})
generate_export_header(krita_tool_svgtext_static BASE_NAME kritatoolsvgtext)

target_include_directories(krita_tool_svgtext_static PRIVATE
    ${CMAKE_SOURCE_DIR}/pk/container
    ${CMAKE_SOURCE_DIR}/pk/concurrent
    ${CMAKE_SOURCE_DIR}/pk/geometry
    ${CMAKE_SOURCE_DIR}/pk/namespace
    ${CMAKE_SOURCE_DIR}/pk/string)

target_link_libraries(krita_tool_svgtext_static
    kritacanvas
    kritaimpex
    kritashapemodel
    pkcontainer
    pkconcurrent
    pkgeometry
    pknamespace
    pkstring
    KF${KF_MAJOR}::I18n
    # qt 桶（-DQT_CORE_LIB、无 compat）下 PkFlakeBridge.h 会 include 真 Qt 的
    # <QtCore/QtCore> <QtGui/QtGui> <QtWidgets/QtWidgets> <QtXml/QtXml> <QtSvg/QtSvg>
    # 五个 umbrella（PkFlakeBridge.h:63-67）。这些 umbrella 需要对应 framework 的
    # Headers 目录在 include 面上（framework 内部头用 <QSurfaceFormat> 这类裸尖括号
    # include，只有 framework 根在搜索路径时解析不到）。本 target 原先只经 KMime/…
    # 传递拿到 QtCore，故 SvgTextCursor.cpp / SvgTextTool.cpp / SvgTextToolFactory.cpp
    # 三个 TU 编不过。补的就是 PkFlakeBridge.h 需要的这四个模块——与 kritaflake
    # （libs/flake/CMakeLists.txt:615/619/620 PUBLIC Qt::Svg / Qt::Gui / Qt::Xml）
    # 及每个链 kritaflake 的 qt 桶工具插件（kritadefaulttools_static 等）实际拿到的
    # 模块集一致。**不新增任何 pk include 面**（判据⑧ 不受影响），也不加 compat 目录。
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Widgets
    Qt${QT_MAJOR_VERSION}::Xml
    Qt${QT_MAJOR_VERSION}::Svg)

target_link_libraries(krita_tool_svgtext PRIVATE krita_tool_svgtext_static)

install(TARGETS krita_tool_svgtext DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
