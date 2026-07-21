# UltraRender 是外部维护的引擎仓库,这里只 vendoring 它的 C ABI 头文件快照
# (来自 ure_core/include/ure/ure_c_api.h),用于前端集成代码的编译。
#
# 实际链接的 libure_core.so / ure_core.lib 由 CI/本地构建时通过环境变量
# UR_ULTRARENDER_LIB_DIR 指定路径,不随这个仓库分发 —— 避免把渲染引擎的
# 构建产物和前端仓库的版本历史绑在一起。
#
# 头文件快照更新流程:UltraRender 那边发布新版本 -> 手动同步这个目录下的
# 头文件 -> 在 docs/architecture/ 里记一条 ADR 说明这次同步对应哪个
# UltraRender commit/tag,以及是否有 ABI 破坏性变更。

add_library(ultrarender_c_api INTERFACE)
add_library(ultrarender::c_api ALIAS ultrarender_c_api)

target_include_directories(ultrarender_c_api INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (DEFINED ENV{UR_ULTRARENDER_LIB_DIR})
    find_library(URE_CORE_LIB
        NAMES ure_core
        PATHS $ENV{UR_ULTRARENDER_LIB_DIR}
        NO_DEFAULT_PATH
    )
    if (URE_CORE_LIB)
        target_link_libraries(ultrarender_c_api INTERFACE ${URE_CORE_LIB})
    else()
        message(WARNING
            "UR_ULTRARENDER_LIB_DIR 已设置但没找到 ure_core 库文件,"
            "ur_scene_bridge 会编译但链接期会失败。")
    endif()
else()
    message(STATUS
        "UR_ULTRARENDER_LIB_DIR 未设置 —— 只编译 ur_scene_bridge 的头文件依赖部分,"
        "不链接真正的 UltraRender 引擎。CI 里跑完整集成测试前需要设置这个变量。")
endif()
