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
    KF${KF_MAJOR}::I18n)

target_link_libraries(krita_tool_svgtext PRIVATE krita_tool_svgtext_static)

install(TARGETS krita_tool_svgtext DESTINATION ${KRITA_PLUGIN_INSTALL_DIR})
