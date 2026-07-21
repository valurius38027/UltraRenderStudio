add_executable(ur_integration_tests
    platform_gfx_integration_test.cpp
)

target_link_libraries(ur_integration_tests
    PRIVATE
    ur::platform
    ur::gfx
    GTest::gtest
)

ur_set_warnings(ur_integration_tests)
add_test(NAME ur_integration_tests COMMAND ur_integration_tests)
set_tests_properties(ur_integration_tests PROPERTIES
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/lsan_suppressions.txt"
)

add_executable(ur_window_present_tests
    window_present_integration_test.cpp
)

target_link_libraries(ur_window_present_tests
    PRIVATE
    ur::platform
    ur::gfx
    GTest::gtest
)

ur_set_warnings(ur_window_present_tests)
add_test(NAME ur_window_present_tests COMMAND ur_window_present_tests)
set_tests_properties(ur_window_present_tests PROPERTIES
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/lsan-vulkan-loader.supp"
)
