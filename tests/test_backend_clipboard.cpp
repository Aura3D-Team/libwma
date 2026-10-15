/*
 * IWindowManager's clipboard on the SDL and GLFW backends, which hand it to the platform's own API: what is set reads
 * back, byte for byte, including text that is not ASCII and a lot of it.
 *
 * The SDL build runs on SDL's dummy video driver, so it needs no display. The GLFW one needs a window: it runs
 * against $WMA_TEST_DISPLAY, a disposable X server, never the desktop's $DISPLAY, and is skipped without one.
 */

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "wma/wma.hpp"

namespace
{

constexpr int kSkipped = 77;

int failures = 0;

void check(bool condition, const char *description)
{
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition)
        ++failures;
}

} // namespace

int main()
{
#if defined(WMA_TEST_BACKEND_SDL)
    constexpr wma::WindowBackend kBackend = wma::WindowBackend::SDL3;
    constexpr wma::GraphicsAPI kApi = wma::GraphicsAPI::CPU;
    setenv("SDL_VIDEODRIVER", "dummy", 1);
#elif defined(WMA_TEST_BACKEND_GLFW)
    constexpr wma::WindowBackend kBackend = wma::WindowBackend::GLFW;
    //! GLFW has no software path; a Vulkan window needs no context, and nothing here draws.
    constexpr wma::GraphicsAPI kApi = wma::GraphicsAPI::Vulkan;
    // Never $DISPLAY: this opens a window, which is not for a live desktop.
    const char *testDisplay = std::getenv("WMA_TEST_DISPLAY");
    if (!testDisplay)
    {
        std::puts("WMA_TEST_DISPLAY unset; skipped");
        return kSkipped;
    }
    setenv("DISPLAY", testDisplay, 1);
    //! GLFW would prefer a Wayland session if the environment names one.
    unsetenv("WAYLAND_DISPLAY");
#else
#error "name a backend"
#endif

    if (!wma::isBackendAvailable(kBackend))
    {
        std::puts("backend not built; skipped");
        return kSkipped;
    }

    try
    {
        wma::WindowDetails details;
        details.width = 320;
        details.height = 200;
        auto window = wma::createWindowManager(kBackend, details, kApi);
        window->createWindow("wma clipboard");
        window->pollEvents();

        check(window->isClipboardSupported(), "the backend has a clipboard");

        const std::string text = "caf\xC3\xA9 \xE2\x9C\x93 \xF0\x9F\x99\x82 line\nbreak\ttab";
        check(window->setClipboardText(text), "setting the clipboard succeeds");
        window->pollEvents();
        check(window->getClipboardText() == text, "and reads back as set, newlines and all");

        check(window->setClipboardText("second") && window->getClipboardText() == "second", "a later set replaces it");

        const std::string embedded("cut\0short", 9);
        check(!window->setClipboardText(embedded) && window->getClipboardText() == "second",
              "text with an embedded NUL is refused rather than cut short, and leaves the clipboard alone");

        const std::string large(1 << 20, 'z');
        check(window->setClipboardText(large) && window->getClipboardText() == large, "a megabyte round-trips");

        check(window->setClipboardText("") && window->getClipboardText().empty(), "empty text reads back as empty");
    }
    catch (const std::exception &error)
    {
        std::printf("[FAIL] exception: %s\n", error.what());
        return 1;
    }

    return failures == 0 ? 0 : 1;
}
