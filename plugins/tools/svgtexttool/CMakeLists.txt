project(SvgTextTool)
add_subdirectory(tests)

set(CMAKE_AUTORCC ON)

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

set(SvgTextTool_SOURCES
    Plugin.cpp
    svgtexttool.qrc
)


kis_add_library(krita_tool_svgtext_static STATIC ${SvgTextTool_SRCS})
kis_add_library(krita_tool_svgtext MODULE ${SvgTextTool_SOURCES})
generate_export_header(krita_tool_svgtext_static BASE_NAME kritatoolsvgtext)

target_link_libraries(krita_tool_svgtext_static
    kritaui
    Qt${QT_MAJOR_VERSION}::Quick
    Qt${QT_MAJOR_VERSION}::Qml
    KF${KF_MAJOR}::I18n)

target_link_libraries(krita_tool_svgtext PRIVATE krita_tool_svgtext_static)

install(TARGETS krita_tool_svgtext DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
