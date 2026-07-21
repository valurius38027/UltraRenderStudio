#pragma once

#include <memory>

namespace ur::scene_bridge {

/// 对 ure_c_api.h 里 ure_session_t 的 RAII 封装。这一层的职责边界很窄:
/// 只负责会话生命周期和最基础的相机更新调用,不做任何 UI 相关的事情,
/// 也不知道节点图长什么样 —— 节点图到 SceneIR 的编译发生在更上层
/// (未来的 ur_scene_bridge/procedural_compiler.hpp,目前尚未实现)。
///
/// 当前只暴露"离线渲染显式触发"这一条路径,对应架构讨论里确认过的
/// 决策:视口用前端自己的 ur_gfx 光栅化,UltraRender 只在用户点击
/// "渲染"时才启动。
class RenderSession {
public:
    // TODO(ur_scene_bridge): 目前只验证 ure_c_api.h 能正确解析、
    // ure_session_create/destroy 能配对调用不崩溃。渲染触发
    // (ure_session_render_pass)、相机/材质更新、SceneIR 加载
    // 等真正的业务接口留给下一阶段实现。
    static std::unique_ptr<RenderSession> create();
    ~RenderSession();

    RenderSession(const RenderSession&) = delete;
    RenderSession& operator=(const RenderSession&) = delete;

    /// 返回 false 表示会话句柄创建失败(比如 CUDA 设备不可用)。
    [[nodiscard]] bool isValid() const;

private:
    RenderSession();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ur::scene_bridge
