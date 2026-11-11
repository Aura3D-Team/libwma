// ResizeWatch: every reader sees every resize, whatever the other readers do. With SDL built, also that a real
// backend counts one: SDL's dummy video driver needs no display.

#include <cstdio>
#include <cstdlib>

#include <wma/wma.hpp>

#if defined(WMA_TEST_HAS_SDL)
#include <SDL3/SDL.h>
#endif

namespace
{

int g_failures = 0;

void check(bool condition, const char *description)
{
    std::printf("%s %s\n", condition ? "ok  " : "FAIL", description);
    if (!condition)
        ++g_failures;
}

} // namespace

int main()
{
    wma::WindowFlags flags;
    wma::ResizeWatch renderer;
    wma::ResizeWatch app;
    check(!renderer.pending(flags) && !renderer.consume(flags), "a window that never resized reports nothing");

    flags.markResized();
    check(renderer.pending(flags), "a resize is pending");
    check(renderer.pending(flags), "pending() marks nothing handled");
    check(renderer.consume(flags), "consume() reports the resize");
    check(!renderer.consume(flags), "and only once");
    check(app.consume(flags), "one reader consuming does not hide the resize from another");

    flags.markResized();
    flags.markResized();
    flags.markResized();
    check(renderer.consume(flags) && !renderer.consume(flags), "a burst between two checks reads as one");

    const wma::ResizeWatch late{flags};
    check(!late.pending(flags), "a watch built from the flags starts at their count");
    flags.markResized();
    check(late.pending(flags), "and sees resizes after that");

    wma::ResizeWatch fresh;
    check(fresh.consume(flags), "a default watch sees every resize already counted");

#if defined(WMA_TEST_HAS_SDL)
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    wma::WindowDetails details;
    details.width = 320;
    details.height = 240;
    auto window = wma::createWindowManager(wma::WindowBackend::SDL3, details, wma::GraphicsAPI::CPU);
    window->createWindow("resize watch");
    window->pollEvents();
    wma::ResizeWatch sdl{*window->getWindowFlags()};

    SDL_SetWindowSize(static_cast<SDL_Window *>(window->getWindowInstance()), 400, 300);
    window->pollEvents();
    check(sdl.consume(*window->getWindowFlags()), "SDL3 counts a resize");
    check(window->getWindowDetails()->width == 400 && window->getWindowDetails()->height == 300,
          "with the new size in the details");
    window->pollEvents();
    check(!sdl.consume(*window->getWindowFlags()), "and no resize where the size stayed");
#endif

    return g_failures == 0 ? 0 : 1;
}
