/*
 * The X11 backend's software path: frames reach the window, two slots bound
 * what is in flight, and it keeps running through a resize or an unmapped
 * window. Runs against $WMA_TEST_DISPLAY, a disposable server (Xvfb, a rootful
 * Xwayland); skipped without one.
 *
 * Which present path ran is printed: MIT-SHM, or XPutImage when the server has
 * no such extension or cannot attach a segment. Both must pass.
 */

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "wma/wma.hpp"

namespace
{

constexpr int kSkipped = 77;
constexpr u32 kColor = 0x00FF8040;

int failures = 0;

void check(bool condition, const char *description)
{
    std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition)
        ++failures;
}

//! Pumps until a slot frees up; a missing completion event shows as a failure rather than a hang.
wma::SoftwareFramebuffer acquire(wma::IWindowManager &window)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const wma::SoftwareFramebuffer frame = window.lockFramebuffer();
        if (frame.valid())
            return frame;
        window.waitEvents(10);
    }
    return {};
}

void fill(const wma::SoftwareFramebuffer &frame, u32 color)
{
    for (i32 y = 0; y < frame.height; ++y)
    {
        auto *row = reinterpret_cast<u32 *>(static_cast<u8 *>(frame.pixels) + static_cast<usize>(y) * frame.pitch);
        for (i32 x = 0; x < frame.width; ++x)
            row[x] = color;
    }
}

bool present(wma::IWindowManager &window, u32 color)
{
    const wma::SoftwareFramebuffer frame = acquire(window);
    if (!frame.valid())
        return false;
    fill(frame, color);
    window.presentFramebuffer();
    return true;
}

//! The window's centre pixel as the server holds it, after it has handled everything sent so far.
u32 readCentre(wma::IWindowManager &window)
{
    auto *display = static_cast<Display *>(window.getNativeDisplayHandle());
    const auto id = reinterpret_cast<Window>(window.getWindowInstance());
    XSync(display, False);
    const wma::WindowDetails *size = window.getWindowDetails();
    XImage *image = XGetImage(display, id, size->width / 2, size->height / 2, 1, 1, AllPlanes, ZPixmap);
    if (!image)
        return ~0u;
    const u32 pixel = static_cast<u32>(XGetPixel(image, 0, 0)) & 0xFFFFFF;
    XDestroyImage(image);
    return pixel;
}

void pumpUntilSize(wma::IWindowManager &window, i32 width, i32 height)
{
    for (int i = 0; i < 100; ++i)
    {
        window.waitEvents(10);
        if (window.getWindowDetails()->width == width && window.getWindowDetails()->height == height)
            return;
    }
}

//! Segments the system still lists; the server detaches asynchronously, so callers settle first.
usize sysvSegments()
{
    std::ifstream table("/proc/sysvipc/shm");
    std::string line;
    usize count = 0;
    while (std::getline(table, line))
        ++count;
    return count;
}

void settle(wma::IWindowManager &window)
{
    for (int i = 0; i < 20; ++i)
        window.waitEvents(5);
}

} // namespace

int main()
{
    // Never $DISPLAY: this opens windows and grabs the pointer, which is not for a live desktop.
    const char *testDisplay = std::getenv("WMA_TEST_DISPLAY");
    if (!testDisplay)
    {
        std::puts("WMA_TEST_DISPLAY unset; skipped");
        return kSkipped;
    }
    setenv("DISPLAY", testDisplay, 1);

    const usize segmentsBefore = sysvSegments();
    {
        wma::WindowDetails details;
        details.width = 320;
        details.height = 240;
        auto window = wma::createWindowManager(wma::WindowBackend::X11, details, wma::GraphicsAPI::CPU);
        window->createWindow("wma x11 cpu present");
        auto *display = static_cast<Display *>(window->getNativeDisplayHandle());
        const auto id = reinterpret_cast<Window>(window->getWindowInstance());
        settle(*window);

        check(present(*window, kColor), "a frame can be presented");
        check(readCentre(*window) == kColor, "the presented pixels reach the window");

        // Two presents with no events read in between: the server has not been heard from.
        check(present(*window, kColor), "second slot is usable while the first is in flight");
        const bool shm = !window->lockFramebuffer().valid();
        std::printf("present path: %s\n", shm ? "MIT-SHM" : "XPutImage");
        if (shm)
            check(true, "a third frame is refused while both slots are in flight");
        settle(*window);
        check(acquire(*window).valid(), "a slot is released once the server reports it");
        window->presentFramebuffer();

        XResizeWindow(display, id, 400, 300);
        pumpUntilSize(*window, 400, 300);
        const wma::SoftwareFramebuffer resized = acquire(*window);
        check(resized.valid() && resized.width == 400 && resized.height == 300, "the framebuffer follows a resize");
        if (resized.valid())
        {
            fill(resized, 0x0000FF00);
            window->presentFramebuffer();
        }
        check(readCentre(*window) == 0x0000FF00, "the resized frame reaches the window");

        // A resize drag: each step needs a new slot, and none may be left behind.
        bool churned = true;
        const auto begin = std::chrono::steady_clock::now();
        for (int step = 0; step < 60 && churned; ++step)
        {
            const i32 width = 300 + (step % 7) * 20;
            XResizeWindow(display, id, static_cast<unsigned>(width), 260);
            pumpUntilSize(*window, width, 260);
            churned = present(*window, kColor);
        }
        const std::chrono::duration<double, std::milli> elapsed = std::chrono::steady_clock::now() - begin;
        std::printf("resize step: %.2f ms\n", elapsed.count() / 60.0);
        check(churned, "sixty resize steps each present a frame");

        // A minimised window is unmapped; completions must still free the slots.
        XUnmapWindow(display, id);
        XSync(display, False);
        bool unmapped = true;
        for (int frame = 0; frame < 30 && unmapped; ++frame)
            unmapped = present(*window, kColor);
        check(unmapped, "frames keep flowing while the window is unmapped");
        XMapWindow(display, id);
        check(present(*window, kColor), "and after it is mapped again");

        window->destroy();
    }
    check(sysvSegments() <= segmentsBefore, "no SysV segment outlives the window");

    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
