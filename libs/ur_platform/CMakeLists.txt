find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_library(ur_platform STATIC
    src/window.cpp
)
add_library(ur::platform ALIAS ur_platform)

target_include_directories(ur_platform
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ur_platform
    PUBLIC Qt6::Core Qt6::Gui
)

ur_set_warnings(ur_platform)
ur_apply_sanitizers(ur_platform)

if (UR_BUILD_TESTS)
    add_executable(ur_platform_tests tests/window_test.cpp)
    target_link_libraries(ur_platform_tests PRIVATE ur::platform GTest::gtest)
    ur_set_warnings(ur_platform_tests)
    add_test(NAME ur_platform_tests COMMAND ur_platform_tests)
    set_tests_properties(ur_platform_tests PROPERTIES
        ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/lsan_suppressions.txt"
    )
endif()
