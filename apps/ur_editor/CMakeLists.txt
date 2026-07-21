add_executable(ur_editor
    src/main.cpp
    src/editor_app.cpp
)

target_link_libraries(ur_editor
    PRIVATE
    ur::platform
    ur::gfx
    ur::widgets
)

# scene_bridge 需要真实的 UltraRender 引擎库才能完整链接(见
# libs/ur_scene_bridge/CMakeLists.txt 里的说明)。只在环境变量指向
# 了真实引擎库时才把它接进最终可执行文件,否则应用外壳依然可以
# 独立编译运行(视口/节点图先用假数据跑通交互,不阻塞前端开发)。
if (DEFINED ENV{UR_ULTRARENDER_LIB_DIR})
    target_link_libraries(ur_editor PRIVATE ur::scene_bridge)
    target_compile_definitions(ur_editor PRIVATE UR_HAS_SCENE_BRIDGE=1)
endif()

ur_set_warnings(ur_editor)
ur_apply_sanitizers(ur_editor)

add_test(NAME ur_editor_smoke COMMAND ur_editor --frames 5)
set_tests_properties(ur_editor_smoke PROPERTIES
    TIMEOUT 15
    PASS_REGULAR_EXPRESSION "presented_frames=[1-9][0-9]*"
    ENVIRONMENT "LSAN_OPTIONS=suppressions=${CMAKE_SOURCE_DIR}/cmake/lsan-vulkan-loader.supp"
)
