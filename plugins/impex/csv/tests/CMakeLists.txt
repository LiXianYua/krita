krita_add_broken_unit_test(
    kis_csv_test.cpp
    TEST_NAME kis_csv_test
    LINK_LIBRARIES kritaimpex kritatestsdk
    )

add_executable(kis_csv_write_all_test csv_write_all_test.cpp)
target_link_libraries(kis_csv_write_all_test PRIVATE kritaimpex)
add_test(NAME kis_csv_write_all_test COMMAND kis_csv_write_all_test)
