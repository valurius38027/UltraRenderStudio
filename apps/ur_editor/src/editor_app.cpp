#include "editor_app.hpp"

#include "editor_config.hpp"
#include "ur/gfx/present.hpp"
#include "ur/platform/events.hpp"
#include "ur/widgets/render.hpp"

#include <QCoreApplication>
#include <QObject>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <variant>

namespace ur::editor {
namespace {

constexpr ur::widgets::Rect kPrimaryButtonRect{40.0F, 40.0F, 220.0F, 72.0F};

}  // namespace

EditorApp::EditorApp(std::uint64_t frameLimit)
    : defaultFont_(textSystem_.loadFont(
          {std::filesystem::path(config::kDefaultFontPath), config::kDefaultFontPixelSize})),
      widgets_(textSystem_, defaultFont_),
      frameLimit_(frameLimit) {
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
        ur::gfx::UiFrame frame = buildFrame(drawList);
        textGlyphs_ = 0U;
        for (const ur::gfx::UiPrimitive& primitive : frame.primitives) {
            if (std::holds_alternative<ur::gfx::MaskedQuadPrimitive>(primitive)) {
                ++textGlyphs_;
            }
        }

        ++attemptedFrames_;
        switch (device_->presentUiFrame(frame)) {
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
    const std::uint64_t atlasRevision = textSystem_.atlas().revision;
    std::printf("UltraRenderStudio summary: attempted_frames=%llu presented_frames=%llu "
                "button_toggled=%s text_glyphs=%llu atlas_revision=%llu\n",
                static_cast<unsigned long long>(attemptedFrames_),
                static_cast<unsigned long long>(presentedFrames_),
                buttonToggled_ ? "true" : "false",
                static_cast<unsigned long long>(textGlyphs_),
                static_cast<unsigned long long>(atlasRevision));
    std::fflush(stdout);
    QCoreApplication::exit(exitCode);
}

ur::gfx::UiFrame EditorApp::buildFrame(const ur::widgets::DrawList& drawList) const {
    ur::gfx::UiFrame frame = ur::widgets::buildUiFrame(drawList, textSystem_.atlas());
    const ur::widgets::WidgetId buttonId = ur::widgets::hashLabel("Primary action");
    if (buttonToggled_ && widgets_.hoveredId() != buttonId && widgets_.activeId() != buttonId &&
        !frame.primitives.empty()) {
        if (auto* background = std::get_if<ur::gfx::RectPrimitive>(&frame.primitives.front())) {
            background->r = 0.15F;
            background->g = 0.55F;
            background->b = 0.30F;
            background->a = 1.0F;
        }
    }
    return frame;
}

}  // namespace ur::editor
