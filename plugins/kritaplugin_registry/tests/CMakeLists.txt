add_executable(register_all_plugins_test
    register_all_plugins_test.cpp
    ../../register_all_plugins.cpp
)

target_include_directories(register_all_plugins_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

add_test(NAME plugins-register-all-plugins-test COMMAND register_all_plugins_test)
