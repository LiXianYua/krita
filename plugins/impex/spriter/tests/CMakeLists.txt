add_executable(KisSpriterFormatTest KisSpriterFormatTest.cpp ../spriter_format.cpp)
target_include_directories(KisSpriterFormatTest PRIVATE ..)
target_link_libraries(KisSpriterFormatTest PRIVATE pkport pkxml pkpointer kritastore)
add_test(NAME KisSpriterFormatTest COMMAND KisSpriterFormatTest)
