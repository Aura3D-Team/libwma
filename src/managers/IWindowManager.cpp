#include "wma/managers/IWindowManager.hpp"
#include "wma/core/FrameTimer.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <memory>
#endif

namespace wma
{

#ifdef __EMSCRIPTEN__

//! On the web a blocking while-loop starves the browser event loop; instead the
//! per-frame body is handed to requestAnimationFrame via emscripten_set_main_loop.
//! The heap-allocated context outlives the call and is freed when the window
//! signals it should close.
namespace
{

struct WebLoopContext
{
    IWindowManager *manager;
    std::function<void()> actions;
    FrameTimer timer;

    WebLoopContext(IWindowManager *m, std::function<void()> &&a)
        : manager(m), actions(std::move(a)), timer(*m->getWindowFlags())
    {
    }
};

void webLoopTick(void *userData)
{
    auto *ctx = static_cast<WebLoopContext *>(userData);

    if (ctx->manager->shouldClose())
    {
        emscripten_cancel_main_loop();
        delete ctx;
        return;
    }

    ctx->timer.beginFrame();
    ctx->manager->pollEvents();
    ctx->actions();
    ctx->manager->swapBuffers();
    ctx->timer.endFrame();
}

} // namespace

void IWindowManager::process(std::function<void()> &&actions)
{
    //! Heap-allocated so it outlives this call. simulate_infinite_loop=true keeps
    //! process() from returning (matching desktop's blocking semantics) so the
    //! caller's window and callback stay alive while the browser drives the loop.
    auto *ctx = new WebLoopContext(this, std::move(actions));
    //! fps = 0 => use requestAnimationFrame (respects the display refresh rate).
    emscripten_set_main_loop_arg(webLoopTick, ctx, 0, /*simulate_infinite_loop=*/true);
}

#else

void IWindowManager::process(std::function<void()> &&actions)
{
    FrameTimer timer(*getWindowFlags());
    timer.setTargetFPS(static_cast<u32>(getWindowDetails()->targetFPS < 0 ? 0 : getWindowDetails()->targetFPS));

    while (!shouldClose())
    {
        timer.beginFrame();
        pollEvents();
        actions();
        swapBuffers();
        timer.endFrame();
    }
}

#endif

} // namespace wma
