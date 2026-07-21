add_library(ur_text STATIC
    src/text.cpp
)
add_library(ur::text ALIAS ur_text)

target_include_directories(ur_text
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ur_text
    PUBLIC
    ur::platform
    ur::gfx
)

ur_set_warnings(ur_text)
ur_apply_sanitizers(ur_text)

if (UR_BUILD_TESTS)
    add_executable(ur_text_tests tests/text_test.cpp)
    target_link_libraries(ur_text_tests PRIVATE ur::text GTest::gtest GTest::gtest_main)
    ur_set_warnings(ur_text_tests)
    add_test(NAME ur_text_tests COMMAND ur_text_tests)
endif()
