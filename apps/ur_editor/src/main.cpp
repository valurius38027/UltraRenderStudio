#include "editor_app.hpp"

#include <QGuiApplication>

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace {

std::uint64_t parseFrameLimit(int argc, char* argv[]) {
    std::uint64_t frameLimit = 0U;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument != "--frames") {
            throw std::invalid_argument("unknown argument: " + std::string(argument));
        }
        if (index + 1 >= argc) {
            throw std::invalid_argument("--frames requires a positive integer");
        }

        const std::string_view value(argv[++index]);
        std::uint64_t parsed = 0U;
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (error != std::errc{} || end != value.data() + value.size() || parsed == 0U) {
            throw std::invalid_argument("--frames requires a positive integer");
        }
        frameLimit = parsed;
    }
    return frameLimit;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const std::uint64_t frameLimit = parseFrameLimit(argc, argv);
        QGuiApplication application(argc, argv);
        ur::editor::EditorApp editor(frameLimit);
        editor.show();
        return QGuiApplication::exec();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "UltraRenderStudio startup failure: %s\n", error.what());
        return 2;
    }
}
