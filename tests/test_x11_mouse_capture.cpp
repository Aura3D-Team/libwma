/*
 * X11 pointer capture: with the cursor disabled the pointer is confined to the
 * window, yet look deltas keep arriving after it has reached the edge -- the
 * property an FPS camera needs. The grab follows focus and goes with the
 * cursor. Needs a disposable server ($WMA_TEST_DISPLAY) with XTest; skipped without one.
 */

#include <cstdio>
#include <cstdlib>

//! First: <X11/extensions/XI.h> (via XTest.h) defines COUNT, which ink::LogLevel uses.
#include "wma/wma.hpp"

#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>

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

void pump(wma::IWindowManager &window)
{
    for (int i = 0; i < 10; ++i)
        window.waitEvents(5);
}

//! Whether another client can take the pointer: it cannot while the window holds a grab.
bool pointerFree(Display *other, Window root)
{
    const int result =
        XGrabPointer(other, root, False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    if (result == GrabSuccess)
        XUngrabPointer(other, CurrentTime);
    XSync(other, False);
    return result == GrabSuccess;
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

    wma::WindowDetails details;
    details.width = 300;
    details.height = 200;
    auto window = wma::createWindowManager(wma::WindowBackend::X11, details, wma::GraphicsAPI::CPU);
    window->createWindow("wma x11 mouse capture");
    auto *display = static_cast<Display *>(window->getNativeDisplayHandle());
    const auto id = reinterpret_cast<Window>(window->getWindowInstance());
    Display *other = XOpenDisplay(nullptr);
    int eventBase = 0;
    int errorBase = 0;
    if (!other || !XTestQueryExtension(display, &eventBase, &errorBase, &eventBase, &errorBase))
    {
        std::puts("no XTest; skipped");
        return kSkipped;
    }
    const Window root = DefaultRootWindow(other);

    int moves = 0;
    f64 total = 0.0;
    window->getMouseListener().setMoveAction(wma::MouseAction{[&](const wma::WMAMousePosition &position)
                                                              {
                                                                  ++moves;
                                                                  total += position.deltaX;
                                                              }});
    window->getMouseListener().setCursorEnabled(false);

    // What a window manager does on map: hand the window the focus.
    XTestFakeMotionEvent(display, -1, 100 + 150, 100 + 100, 0);
    XSetInputFocus(display, id, RevertToParent, CurrentTime);
    pump(*window);
    check(!pointerFree(other, root), "the pointer is grabbed once the window has focus");

    // Far more travel than the window is wide: the pointer stops at the edge, the deltas must not.
    pump(*window);
    moves = 0;
    total = 0.0;
    for (int i = 0; i < 20; ++i)
    {
        XTestFakeRelativeMotionEvent(display, 50, 0, 0);
        XSync(display, False);
        pump(*window);
    }
    const int movesAtEdge = moves;
    check(total > details.width * 2, "deltas keep arriving past the window edge");

    moves = 0;
    for (int i = 0; i < 5; ++i)
    {
        XTestFakeRelativeMotionEvent(display, 50, 0, 0);
        XSync(display, False);
        pump(*window);
    }
    check(movesAtEdge > 0 && moves == 5, "every further step at the edge is still reported");

    XSetInputFocus(display, None, RevertToNone, CurrentTime);
    pump(*window);
    check(pointerFree(other, root), "losing focus releases the grab");

    XSetInputFocus(display, id, RevertToParent, CurrentTime);
    pump(*window);
    check(!pointerFree(other, root), "regaining focus takes it again");

    window->getMouseListener().setCursorEnabled(true);
    pump(*window);
    check(pointerFree(other, root), "enabling the cursor releases the grab");

    XCloseDisplay(other);
    window->destroy();
    std::printf("%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
