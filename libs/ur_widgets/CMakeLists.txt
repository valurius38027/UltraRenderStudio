add_library(ur_widgets STATIC
    src/context.cpp
    src/render.cpp
)
add_library(ur::widgets ALIAS ur_widgets)

target_include_directories(ur_widgets
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ur_widgets
    PUBLIC
    ur::platform
    ur::gfx
    ur::text
)

ur_set_warnings(ur_widgets)
ur_apply_sanitizers(ur_widgets)

if (UR_BUILD_TESTS)
    add_executable(ur_widgets_tests tests/context_test.cpp)
    target_link_libraries(ur_widgets_tests PRIVATE ur::widgets GTest::gtest GTest::gtest_main)
    ur_set_warnings(ur_widgets_tests)
    add_test(NAME ur_widgets_tests COMMAND ur_widgets_tests)

    # 独立可执行文件: 需要真实 QGuiApplication + ur_gfx Vulkan 设备,
    # 和 context_test.cpp 的纯逻辑单元测试(不需要 Qt 事件循环)分开,
    # 避免两边对 main() / QGuiApplication 生命周期的假设互相冲突。
    add_executable(ur_widgets_render_tests tests/render_integration_test.cpp)
    target_link_libraries(ur_widgets_render_tests PRIVATE ur::widgets GTest::gtest)
    ur_set_warnings(ur_widgets_render_tests)
    add_test(NAME ur_widgets_render_tests COMMAND ur_widgets_render_tests)
    set_tests_properties(ur_widgets_render_tests PROPERTIES
        ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/lsan_suppressions.txt"
    )
endif()
