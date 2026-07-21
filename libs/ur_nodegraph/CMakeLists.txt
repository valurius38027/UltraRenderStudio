add_library(ur_nodegraph STATIC
    src/nodegraph.cpp
)
add_library(ur::nodegraph ALIAS ur_nodegraph)

target_include_directories(ur_nodegraph
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ur_nodegraph
    PUBLIC
    ur::widgets
)

ur_set_warnings(ur_nodegraph)
ur_apply_sanitizers(ur_nodegraph)

if (UR_BUILD_TESTS)
    add_executable(ur_nodegraph_tests tests/nodegraph_test.cpp)
    target_link_libraries(ur_nodegraph_tests PRIVATE ur::nodegraph GTest::gtest GTest::gtest_main)
    ur_set_warnings(ur_nodegraph_tests)
    add_test(NAME ur_nodegraph_tests COMMAND ur_nodegraph_tests)
endif()
