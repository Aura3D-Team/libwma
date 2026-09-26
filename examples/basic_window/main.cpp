// Comprehensive tour of the wma public API. The same source builds unmodified
// for desktop, Android and WASM (see examples/CMakeLists.txt) because it picks
// its backend via wma::getDefaultBackend() and renders with GraphicsAPI::CPU,
// which needs no GPU loader/SDK and is supported everywhere.

#include <ink/Inkogger.h>

#include "wma/wma.hpp"

namespace
{

const char *toString(wma::WindowBackend backend)
{
    switch (backend)
    {
    case wma::WindowBackend::GLFW:
        return "GLFW";
    case wma::WindowBackend::SDL3:
        return "SDL3";
    case wma::WindowBackend::X11:
        return "X11";
    case wma::WindowBackend::WAYLAND:
        return "Wayland";
    default:
        return "Unknown";
    }
}

const char *toString(wma::GraphicsAPI api)
{
    switch (api)
    {
    case wma::GraphicsAPI::OpenGL:
        return "OpenGL";
    case wma::GraphicsAPI::Vulkan:
        return "Vulkan";
    case wma::GraphicsAPI::CPU:
        return "CPU (software)";
    case wma::GraphicsAPI::Metal:
        return "Metal";
    default:
        return "Unknown";
    }
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    try
    {
        INK_LOG << wma::getLibraryInfo();

        // Compile-time feature detection (compile definitions on wma::wma)
        INK_LOG << "Compiled backends -> GLFW:" << WMA_HAS_GLFW << " SDL3:" << WMA_HAS_SDL << " X11:" << WMA_HAS_X11
                << " Wayland:" << WMA_HAS_WAYLAND;

        // Runtime feature detection
        for (auto backend :
             {wma::WindowBackend::GLFW, wma::WindowBackend::SDL3, wma::WindowBackend::X11, wma::WindowBackend::WAYLAND})
        {
            INK_LOG << "  isBackendAvailable(" << toString(backend) << ") = " << wma::isBackendAvailable(backend);
        }

        // getDefaultBackend() prefers SDL3 when it's compiled in — Platform.cmake
        // forces SDL3-only on Android/WASM, so this line needs no #ifdef to be
        // portable across desktop, Android and the web.
        const wma::WindowBackend backend = wma::getDefaultBackend();
        INK_LOG << "Selected backend: " << toString(backend);

        // CPU rendering needs no GPU loader/SDK and every backend supports it,
        // making it the most portable choice for a single cross-platform example.
        // (Flip to OpenGL/Vulkan to exercise the interop calls further below —
        // this example does not bundle a GL loader or the Vulkan SDK, so it only
        // demonstrates *reaching* those APIs, not drawing with them.)
        constexpr wma::GraphicsAPI kGraphicsAPI = wma::GraphicsAPI::CPU;

        // WindowDetails is an aggregate: designated initialization works...
        wma::WindowDetails windowConfig{
            .width = 1280,
            .height = 720,
            .resizable = true,
            .targetFPS = 60, // 0 = unlimited
            .fullscreen = false,
        };
        // ...and so does the positional form: wma::WindowDetails(1280, 720, true, 60);

        auto windowManager = wma::createWindowManager(backend, windowConfig, kGraphicsAPI);
        windowManager->createWindow("wma Example - Basic Window");

        INK_LOG << "Backend:  " << toString(windowManager->getBackendType());
        INK_LOG << "Graphics: " << toString(windowManager->getGraphicsAPI());

        // Graphics-API interop surface
        if (windowManager->getGraphicsAPI() == wma::GraphicsAPI::Vulkan)
        {
            for (const char *ext : windowManager->getVulkanExtensions())
            {
                INK_LOG << "  required Vulkan instance extension: " << ext;
            }
        }
        else if (windowManager->getGraphicsAPI() == wma::GraphicsAPI::OpenGL)
        {
            void *glClearAddr = windowManager->getGLProcAddress("glClear");
            INK_LOG << "  glClear resolved via getGLProcAddress: " << (glClearAddr != nullptr);
        }
        else if (windowManager->getGraphicsAPI() == wma::GraphicsAPI::Metal)
        {
            //! The CAMetalLayer is the whole interop surface for Metal: a renderer
            //! sets its device and pixel format, then draws to its drawables.
            INK_LOG << "  CAMetalLayer: " << windowManager->getMetalLayer();
        }

        INK_LOG << "  native window handle:  " << windowManager->getWindowInstance();
        INK_LOG << "  native display handle: " << windowManager->getNativeDisplayHandle()
                << " (non-null only for X11/Wayland)";

        // Keyboard
        auto &keyboard = windowManager->getKeyboardListener();

        const auto gameplay = keyboard.createContext();
        const auto menu = keyboard.createContext();
        const auto pause = keyboard.createContext(); //! used as a pushed overlay, below

        // clearKeyActions(context): scratch-context demo, does not touch
        //    anything used later (gameplay/menu/pause are untouched).=
        {
            const auto scratch = keyboard.createContext();
            keyboard.addKeyAction(wma::Key::KEY_0,
                                  wma::KeyAction{[]()
                                                 {
                                                 }},
                                  scratch);
            INK_LOG << "  scratch context bound before clear: " << keyboard.hasKeyAction(wma::Key::KEY_0, scratch);
            keyboard.clearKeyActions(scratch);
            INK_LOG << "  scratch context bound after clearKeyActions: "
                    << keyboard.hasKeyAction(wma::Key::KEY_0, scratch);
        }

        keyboard.addKeyAction(wma::Key::KEY_ESCAPE, wma::KeyAction{[&]()
                                                                   {
                                                                       windowManager->destroy();
                                                                       INK_LOG << "Escape pressed - closing window";
                                                                   },
                                                                   nullptr});

        // Per-context bindings: the same key means different things depending
        // on which context is currently resolved.
        keyboard.addKeyAction(wma::Key::KEY_D,
                              wma::KeyAction{[]()
                                             {
                                                 INK_LOG << "D - gameplay: move right";
                                             },
                                             nullptr},
                              gameplay);

        keyboard.addKeyAction(wma::Key::KEY_D,
                              wma::KeyAction{[]()
                                             {
                                                 INK_LOG << "D - menu: select next";
                                             },
                                             nullptr},
                              menu);

        // Flat switch between two top-level contexts.
        keyboard.addKeyAction(wma::Key::KEY_TAB, wma::KeyAction{[&]()
                                                                {
                                                                    if (keyboard.getActiveContext() == gameplay)
                                                                    {
                                                                        keyboard.setActiveContext(menu);
                                                                        INK_LOG << "Switched to menu context";
                                                                    }
                                                                    else
                                                                    {
                                                                        keyboard.setActiveContext(gameplay);
                                                                        INK_LOG << "Switched to gameplay context";
                                                                    }
                                                                },
                                                                nullptr});

        // Stack overlay: pushContext()/popContext() layers "pause" on top of
        // whichever of gameplay/menu is active, without disturbing that choice.
        bool paused = false;
        keyboard.addKeyAction(wma::Key::KEY_P, wma::KeyAction{[&]()
                                                              {
                                                                  paused = !paused;
                                                                  if (paused)
                                                                  {
                                                                      keyboard.pushContext(pause);
                                                                      INK_LOG << "Paused (context pushed)";
                                                                  }
                                                                  else
                                                                  {
                                                                      keyboard.popContext();
                                                                      INK_LOG << "Resumed (context popped)";
                                                                  }
                                                                  INK_LOG
                                                                      << "  active=" << keyboard.getActiveContext()
                                                                      << " resolved=" << keyboard.getResolvedContext();
                                                              },
                                                              nullptr});
        keyboard.addKeyAction(wma::Key::KEY_ENTER,
                              wma::KeyAction{[]()
                                             {
                                                 INK_LOG << "Enter - pause menu: resume";
                                             },
                                             nullptr},
                              pause);

        keyboard.setActiveContext(gameplay);

        // Press + release callbacks on the same binding.
        keyboard.addKeyAction(wma::Key::KEY_SPACE, wma::KeyAction{[]()
                                                                  {
                                                                      INK_LOG << "Space pressed";
                                                                  },
                                                                  []()
                                                                  {
                                                                      INK_LOG << "Space released";
                                                                  }});

        // The i32 overloads accept the same Key values as raw integers — handy
        // when keybinds are loaded from config/network as plain numbers.
        keyboard.addKeyAction(static_cast<i32>(wma::Key::KEY_G),
                              wma::KeyAction{[]()
                                             {
                                                 INK_LOG << "G pressed (bound via the raw-int overload)";
                                             },
                                             nullptr});

        // hasKeyAction()/removeKeyAction(): bind, query, then unbind at runtime.
        keyboard.addKeyAction(wma::Key::KEY_F, wma::KeyAction{[]()
                                                              {
                                                                  INK_LOG << "F pressed - unreachable, removed below";
                                                              },
                                                              nullptr});
        INK_LOG << "  F bound: " << keyboard.hasKeyAction(wma::Key::KEY_F);
        keyboard.removeKeyAction(wma::Key::KEY_F);
        INK_LOG << "  F bound after removeKeyAction: " << keyboard.hasKeyAction(wma::Key::KEY_F);

        // ==================== Mouse ====================
        auto &mouse = windowManager->getMouseListener();
        INK_LOG << "  mouse resolved context: " << mouse.getResolvedContext();

        // clearAllActions(context): same scratch-context pattern as above.
        {
            const auto scratch = mouse.createContext();
            mouse.addButtonAction(wma::MouseButton::WMAButton4,
                                  wma::MouseAction{[]()
                                                   {
                                                   }},
                                  scratch);
            INK_LOG << "  scratch mouse binding before clear: "
                    << mouse.hasButtonAction(wma::MouseButton::WMAButton4, scratch);
            mouse.clearAllActions(scratch);
            INK_LOG << "  scratch mouse binding after clearAllActions: "
                    << mouse.hasButtonAction(wma::MouseButton::WMAButton4, scratch);
        }

        mouse.addButtonAction(wma::MouseButton::WMALeft, wma::MouseAction{[]()
                                                                          {
                                                                              INK_LOG << "mouse left pressed";
                                                                          },
                                                                          []()
                                                                          {
                                                                              INK_LOG << "mouse left released";
                                                                          }});

        mouse.addButtonAction(wma::MouseButton::WMARight,
                              wma::MouseAction{[&]()
                                               {
                                                   mouse.setCursorEnabled(!mouse.isCursorEnabled());
                                                   INK_LOG << "Cursor enabled: " << mouse.isCursorEnabled();
                                               },
                                               nullptr});
        INK_LOG << "  left button bound: " << mouse.hasButtonAction(wma::MouseButton::WMALeft);

        mouse.setMoveAction(wma::MouseAction{[&](const wma::WMAMousePosition &pos)
                                             {
                                                 // Throttled: per-pixel motion logging would flood the console
                                                 // (especially the browser console on WASM).
                                                 static unsigned long long moveEvents = 0;
                                                 if ((++moveEvents % 60) == 0)
                                                 {
                                                     INK_LOG << "mouse move " << pos.x << "," << pos.y << " delta "
                                                             << pos.deltaX << "," << pos.deltaY;
                                                 }
                                             }});

        mouse.setScrollAction(wma::MouseAction{[&](const wma::WMAMouseScroll &offset)
                                               {
                                                   const f64 newSensitivity =
                                                       INK_MAX(0.1, mouse.getSensitivity() + offset.yOffset * 0.1);
                                                   mouse.setSensitivity(newSensitivity);
                                                   INK_LOG << "scroll " << offset.xOffset << "," << offset.yOffset
                                                           << " -> sensitivity " << mouse.getSensitivity();
                                               }});

        // Main loop
        unsigned long long frameCount = 0;
        f64 fpsLogTimer = 0.0;

        windowManager->process(
            [&]()
            {
                frameCount++;

                auto *flags = windowManager->getWindowFlags();
                fpsLogTimer += flags->deltaTime;

                if (flags->resized)
                {
                    const auto *details = windowManager->getWindowDetails();
                    INK_LOG << "Window resized to " << details->width << "x" << details->height;
                    flags->resized = false;
                }

                // windowFlags_ exposes deltaTime/fps every frame; log once/second
                // along with focus/minimize state.
                if (fpsLogTimer >= 1000.0)
                {
                    INK_LOG << "fps: " << flags->fps << " (dt " << flags->deltaTime << " ms)"
                            << " minimized:" << flags->minimized << " focused:" << flags->focused
                            << " cursorPos:" << mouse.getCurrentPosition().x << "," << mouse.getCurrentPosition().y;
                    fpsLogTimer = 0.0;
                }

                // Software rendering: paint an animated gradient into the framebuffer.
                // Walked serially, one row at a time -- wma hands back the raw surface
                // and stops there; a real renderer wanting this in parallel would
                // spread it across its own worker pool, the way Aura3D's
                // CpuFrameBufferManager does, rather than wma owning one on its behalf.
                wma::SoftwareFramebuffer fb = windowManager->lockFramebuffer();
                if (fb.valid())
                {
                    const auto t = static_cast<u32>(frameCount);
                    for (i32 y = 0; y < fb.height; ++y)
                    {
                        auto *row =
                            reinterpret_cast<u32 *>(static_cast<u8 *>(fb.pixels) + static_cast<size_t>(y) * fb.pitch);
                        for (i32 x = 0; x < fb.width; ++x)
                        {
                            const u32 r = static_cast<u32>(x + static_cast<i32>(t)) & 0xFF;
                            const u32 g = static_cast<u32>(y + static_cast<i32>(t)) & 0xFF;
                            const u32 b = (t >> 1) & 0xFF;
                            row[x] = (r << 16) | (g << 8) | b; // XRGB8888
                        }
                    }
                    windowManager->presentFramebuffer();
                }
            });

        // Note: on WASM, process() drives the browser's requestAnimationFrame
        // loop and unwinds the stack instead of returning, so this line is
        // desktop/Android-only in practice.
        INK_LOG << "Window closed successfully. Total frames: " << frameCount;

        // Most-derived exception types first — they all derive from WMAException.
    }
    catch (const wma::WindowException &e)
    {
        INK_LOG << "wma Window Error: " << e.what();
        return -1;
    }
    catch (const wma::GraphicsException &e)
    {
        INK_LOG << "wma Graphics Error: " << e.what();
        return -1;
    }
    catch (const wma::InputException &e)
    {
        INK_LOG << "wma Input Error: " << e.what();
        return -1;
    }
    catch (const wma::WMAException &e)
    {
        INK_LOG << "wma Error: " << e.what();
        return -1;
    }
    catch (const std::exception &e)
    {
        INK_LOG << "Error: " << e.what();
        return -1;
    }

    return 0;
}
