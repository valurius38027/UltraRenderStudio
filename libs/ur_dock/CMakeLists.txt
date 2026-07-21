add_library(ur_dock STATIC
    src/dock.cpp
)
add_library(ur::dock ALIAS ur_dock)

target_include_directories(ur_dock
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ur_dock
    PUBLIC
    ur::widgets
)

ur_set_warnings(ur_dock)
ur_apply_sanitizers(ur_dock)

if (UR_BUILD_TESTS)
    add_executable(ur_dock_tests tests/dock_test.cpp)
    target_link_libraries(ur_dock_tests PRIVATE ur::dock GTest::gtest GTest::gtest_main)
    ur_set_warnings(ur_dock_tests)
    add_test(NAME ur_dock_tests COMMAND ur_dock_tests)
endif()
