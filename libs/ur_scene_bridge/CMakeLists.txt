add_library(ur_scene_bridge STATIC
    src/session.cpp
)
add_library(ur::scene_bridge ALIAS ur_scene_bridge)

target_include_directories(ur_scene_bridge
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(ur_scene_bridge
    PUBLIC ur::platform
    PUBLIC ultrarender::c_api
)

ur_set_warnings(ur_scene_bridge)
ur_apply_sanitizers(ur_scene_bridge)

# 静态库本身总能编译 —— ure_session_create/destroy 的引用在 .a 里是未解析符号,
# 这在静态库阶段是合法的。只有到"最终可执行文件必须链接完整"这一步才需要
# 真正的 UltraRender 引擎库。因此测试/app 可执行文件的链接门槛设在这里,
# 而不是挡在库本身的编译阶段 —— 这样即使暂时没有引擎二进制,团队也能继续
# 开发/测试 ur_scene_bridge 之外的所有前端模块。
if (UR_BUILD_TESTS AND DEFINED ENV{UR_ULTRARENDER_LIB_DIR})
    add_executable(ur_scene_bridge_tests tests/session_test.cpp)
    target_link_libraries(ur_scene_bridge_tests PRIVATE ur::scene_bridge GTest::gtest GTest::gtest_main)
    ur_set_warnings(ur_scene_bridge_tests)
    add_test(NAME ur_scene_bridge_tests COMMAND ur_scene_bridge_tests)
elseif(UR_BUILD_TESTS)
    message(STATUS
        "ur_scene_bridge_tests 跳过构建: 未设置 UR_ULTRARENDER_LIB_DIR,"
        "没有可链接的真实引擎库。库本身(ur_scene_bridge target)仍会正常编译。")
endif()
