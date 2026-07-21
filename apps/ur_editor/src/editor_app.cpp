#include "editor_app.hpp"

#include "ur/gfx/present.hpp"
#include "ur/platform/events.hpp"

#include <QCoreApplication>
#include <QObject>

#include <cstdio>
#include <exception>

namespace ur::editor {
namespace {

constexpr ur::widgets::Rect kPrimaryButtonRect{40.0F, 40.0F, 220.0F, 72.0F};
constexpr std::size_t kPrimaryButtonCommandIndex = 0U;

}  // namespace

EditorApp::EditorApp(std::uint64_t frameLimit) : frameLimit_(frameLimit) {
    window_.resize({1280U, 720U});
    device_ = ur::gfx::RenderDevice::createForWindow(ur::gfx::Backend::Vulkan, window_);

    frameTimer_.setInterval(0);
    frameTimer_.setTimerType(Qt::PreciseTimer);
    QObject::connect(&frameTimer_, &QTimer::timeout, [this] { tick(); });
}

void EditorApp::show() {
    window_.show();
    frameTimer_.start();
}

void EditorApp::tick() {
    if (finished_) {
        return;
    }

    try {
        drainEvents();
        if (finished_ || !window_.isExposed()) {
            return;
        }

        if (resizePending_) {
            device_->resizeSwapChain();
            resizePending_ = false;
        }

        widgets_.beginFrame(pointerX_, pointerY_, leftButtonDown_);
        if (widgets_.button("Primary action", kPrimaryButtonRect)) {
            buttonToggled_ = !buttonToggled_;
        }
        const ur::widgets::DrawList& drawList = widgets_.endFrame();
        const std::vector<ur::gfx::RectPrimitive> primitives = buildFramePrimitives(drawList);

        ++attemptedFrames_;
        switch (device_->presentRects(primitives)) {
            case ur::gfx::PresentResult::Presented:
                ++presentedFrames_;
                break;
            case ur::gfx::PresentResult::Resized:
                resizePending_ = true;
                break;
            case ur::gfx::PresentResult::SkippedNotExposed:
                break;
            case ur::gfx::PresentResult::DeviceLost:
                std::fprintf(stderr, "UltraRenderStudio: Vulkan device lost\n");
                finish(2);
                return;
        }

        if (frameLimit_ != 0U && presentedFrames_ >= frameLimit_) {
            finish(0);
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "UltraRenderStudio frame failure: %s\n", error.what());
        finish(2);
    }
}

void EditorApp::drainEvents() {
    ur::platform::WindowEvent event;
    while (window_.pollEvent(event)) {
        switch (event.type) {
            case ur::platform::WindowEventType::Exposed:
            case ur::platform::WindowEventType::Resized:
                resizePending_ = true;
                break;
            case ur::platform::WindowEventType::PointerMoved:
                pointerX_ = event.x;
                pointerY_ = event.y;
                break;
            case ur::platform::WindowEventType::PointerButtonChanged:
                pointerX_ = event.x;
                pointerY_ = event.y;
                if (event.button == ur::platform::PointerButton::Left) {
                    leftButtonDown_ = event.pressed;
                }
                break;
            case ur::platform::WindowEventType::FocusLost:
                leftButtonDown_ = false;
                widgets_.cancelPointerCapture();
                break;
            case ur::platform::WindowEventType::CloseRequested:
                finish(0);
                return;
        }
    }
}

void EditorApp::finish(int exitCode) {
    if (finished_) {
        return;
    }
    finished_ = true;
    frameTimer_.stop();
    std::printf("UltraRenderStudio summary: attempted_frames=%llu presented_frames=%llu "
                "button_toggled=%s\n",
                static_cast<unsigned long long>(attemptedFrames_),
                static_cast<unsigned long long>(presentedFrames_),
                buttonToggled_ ? "true" : "false");
    std::fflush(stdout);
    QCoreApplication::exit(exitCode);
}

std::vector<ur::gfx::RectPrimitive> EditorApp::buildFramePrimitives(
    const ur::widgets::DrawList& drawList) const {
    std::vector<ur::gfx::RectPrimitive> primitives;
    primitives.reserve(drawList.commands().size());

    const ur::widgets::WidgetId buttonId = ur::widgets::hashLabel("Primary action");
    for (std::size_t index = 0U; index < drawList.commands().size(); ++index) {
        const ur::widgets::RectCommand& command = drawList.commands()[index];
        ur::widgets::Color color = command.color;
        if (index == kPrimaryButtonCommandIndex && buttonToggled_ && widgets_.hoveredId() != buttonId &&
            widgets_.activeId() != buttonId) {
            color = ur::widgets::Color{0.15F, 0.55F, 0.30F, 1.0F};
        }
        primitives.push_back(ur::gfx::RectPrimitive{
            command.rect.x, command.rect.y, command.rect.width, command.rect.height,
            color.r, color.g, color.b, color.a,
        });
    }
    return primitives;
}

}  // namespace ur::editor
