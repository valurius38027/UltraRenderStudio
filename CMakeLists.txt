cmake_minimum_required(VERSION 3.25)

project(UltraRenderStudio
    VERSION 0.1.0
    DESCRIPTION "Frontend editor for the UltraRender procedural rendering engine"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

option(UR_ENABLE_SANITIZERS "Enable ASan/UBSan in this build" OFF)
option(UR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(UR_BUILD_TESTS "Build unit + integration tests" ON)
option(UR_ENABLE_D3D12_BACKEND "Enable D3D12 QRhi backend (Windows only)" OFF)

if (UR_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
endif()

# 外部引擎的头文件快照(不含实际引擎库,见 third_party/ultrarender/CMakeLists.txt 说明)
add_subdirectory(third_party/ultrarender)

# 每个模块是独立的 CMake target,依赖方向必须遵守
# cmake/module_dependency_lint.py 里 ALLOWED_DEPS 表定义的分层规则。
# 新增模块时,两边都要同步更新。
add_subdirectory(libs/ur_platform)
add_subdirectory(libs/ur_gfx)
add_subdirectory(libs/ur_text)
add_subdirectory(libs/ur_widgets)
add_subdirectory(libs/ur_dock)
add_subdirectory(libs/ur_nodegraph)
add_subdirectory(libs/ur_viewport)
add_subdirectory(libs/ur_scene_bridge)

add_subdirectory(apps/ur_editor)

if (UR_BUILD_TESTS)
    add_subdirectory(tests/integration)
endif()

# --- 依赖分层检查,配置阶段就跑一遍(CI 里也会独立跑一次作为门禁) ---
find_package(Python3 COMPONENTS Interpreter QUIET)
if (Python3_FOUND)
    add_custom_target(ur_lint_deps ALL
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/cmake/module_dependency_lint.py
                --repo-root ${CMAKE_SOURCE_DIR}
        COMMENT "Checking module dependency layering rules"
        VERBATIM
    )
endif()
