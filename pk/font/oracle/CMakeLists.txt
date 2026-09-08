cmake_minimum_required(VERSION 3.16)
project(pkfont_oracle C CXX)
set(CMAKE_CXX_STANDARD 17)
enable_testing()

# Qt is confined to this independent test executable. pkfont remains Qt-free.
find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui)
add_subdirectory(.. ${CMAKE_CURRENT_BINARY_DIR}/native)
add_executable(brush_text_native_cases brush_text_cases.cpp)
target_link_libraries(brush_text_native_cases PRIVATE pkfont)
add_executable(brush_text_qt_cases brush_text_cases.cpp)
target_compile_definitions(brush_text_qt_cases PRIVATE PK_FONT_QT_ORACLE)
target_link_libraries(brush_text_qt_cases PRIVATE Qt5::Core Qt5::Gui)
add_test(NAME font_pixels_qt_comparison
    COMMAND ${CMAKE_COMMAND}
        -DNATIVE=$<TARGET_FILE:brush_text_native_cases>
        -DORACLE=$<TARGET_FILE:brush_text_qt_cases>
        -DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/compare.cmake)
set_tests_properties(font_pixels_qt_comparison PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
