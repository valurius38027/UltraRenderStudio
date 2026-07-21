#pragma once

#include "ur/gfx/render_device.hpp"
#include "ur/platform/window.hpp"
#include "ur/text/text.hpp"
#include "ur/widgets/context.hpp"

#include <QTimer>

#include <cstdint>
#include <memory>

namespace ur::editor {

class EditorApp {
public:
    explicit EditorApp(std::uint64_t frameLimit = 0U);
    ~EditorApp() = default;

    EditorApp(const EditorApp&) = delete;
    EditorApp& operator=(const EditorApp&) = delete;

    void show();

private:
    void tick();
    void drainEvents();
    void finish(int exitCode);
    [[nodiscard]] ur::gfx::UiFrame buildFrame(const ur::widgets::DrawList& drawList) const;

    ur::platform::Window window_;
    std::unique_ptr<ur::gfx::RenderDevice> device_;
    ur::text::TextSystem textSystem_;
    ur::text::FontId defaultFont_ = ur::text::kInvalidFontId;
    ur::widgets::Context widgets_;
    QTimer frameTimer_;

    float pointerX_ = 0.0F;
    float pointerY_ = 0.0F;
    bool leftButtonDown_ = false;
    bool resizePending_ = true;
    bool buttonToggled_ = false;
    bool finished_ = false;

    std::uint64_t frameLimit_ = 0U;
    std::uint64_t attemptedFrames_ = 0U;
    std::uint64_t presentedFrames_ = 0U;
    std::uint64_t textGlyphs_ = 0U;
};

}  // namespace ur::editor
