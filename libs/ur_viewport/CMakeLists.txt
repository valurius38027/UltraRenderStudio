add_library(ur_viewport STATIC
    src/viewport.cpp
)
add_library(ur::viewport ALIAS ur_viewport)

target_include_directories(ur_viewport
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ur_viewport
    PUBLIC
    ur::widgets
)

ur_set_warnings(ur_viewport)
ur_apply_sanitizers(ur_viewport)

if (UR_BUILD_TESTS)
    add_executable(ur_viewport_tests tests/viewport_test.cpp)
    target_link_libraries(ur_viewport_tests PRIVATE ur::viewport GTest::gtest GTest::gtest_main)
    ur_set_warnings(ur_viewport_tests)
    add_test(NAME ur_viewport_tests COMMAND ur_viewport_tests)
endif()
