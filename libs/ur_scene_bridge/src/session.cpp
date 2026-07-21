#include "ur/scene_bridge/session.hpp"

#include <ure/ure_c_api.h>

namespace ur::scene_bridge {

struct RenderSession::Impl {
    ure_session_t* handle = nullptr;

    ~Impl() {
        if (handle != nullptr) {
            ure_session_destroy(handle);
        }
    }
};

RenderSession::RenderSession() : impl_(std::make_unique<Impl>()) {
    impl_->handle = ure_session_create();
}

RenderSession::~RenderSession() = default;

std::unique_ptr<RenderSession> RenderSession::create() {
    // std::make_unique 需要访问 private 构造函数,这里手动 new + wrap 绕开这个限制,
    // 保持构造函数对外不可见(只能通过 create() 拿到实例)。
    return std::unique_ptr<RenderSession>(new RenderSession());
}

bool RenderSession::isValid() const {
    return impl_->handle != nullptr;
}

}  // namespace ur::scene_bridge
