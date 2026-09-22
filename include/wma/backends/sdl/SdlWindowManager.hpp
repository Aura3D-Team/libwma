#ifndef WMA_BACKENDS_SDL_WINDOW_MANAGER_HPP
#define WMA_BACKENDS_SDL_WINDOW_MANAGER_HPP

#include <memory>

#include "SDLMouseListener.hpp"
#include "SDLKeyboardListener.hpp"
#include "SDLTouchListener.hpp"
#include "wma/managers/IWindowManager.hpp"

struct SDL_Window;
union SDL_Event;
struct SDL_KeyboardEvent;

namespace wma {

    class SdlWindowManager : public IWindowManager {
    public:
        explicit SdlWindowManager(const WindowDetails& windowDetails,
                                  GraphicsAPI graphicsAPI = GraphicsAPI::Vulkan);
        ~SdlWindowManager() override;

        SdlWindowManager(const SdlWindowManager&) = delete;
        SdlWindowManager& operator=(const SdlWindowManager&) = delete;
        SdlWindowManager(SdlWindowManager&&) noexcept;
        SdlWindowManager& operator=(SdlWindowManager&&) noexcept;

        void createWindow(const char* windowName) override;
        void pollEvents() override;
        void waitEvents(int timeoutMs) override;
        void swapBuffers() override;
        void* getWindowInstance() override;
        bool waitUntilWindowReady() override;
        [[nodiscard]] bool isSurfaceAvailable() const override;
        void* getNativeDisplayHandle() const noexcept override;
        void* getGLProcAddress(const char* name) const override;
        [[nodiscard]] void* getMetalLayer() const noexcept override;
        SoftwareFramebuffer lockFramebuffer() override;
        void presentFramebuffer() override;
        u64 getSDLWindowFlags() const;
        WindowFlags* getWindowFlags() noexcept override;
        [[nodiscard]] FramebufferSize getFramebufferSize() noexcept override;
        const WindowDetails* getWindowDetails() noexcept override;
        const std::vector<const char*> getVulkanExtensions() const override;
        KeyboardListener& getKeyboardListener() noexcept override;
        MouseListener& getMouseListener() noexcept override;
        TouchListener& getTouchListener() noexcept override;
        void setTextInputEnabled(bool enabled) noexcept override;
        [[nodiscard]] bool isTextInputEnabled() const noexcept override;
        bool shouldClose() const override;
        WindowBackend getBackendType() const override;
        GraphicsAPI getGraphicsAPI() const override;
        WmaCode destroy() override;

    private:
        SDL_Window* window_;
        void* glContext_; //!< SDL_GLContext (opaque) — OpenGL mode only
        void* windowSurface_; //!< SDL_Surface* held between lock/present (CPU mode)

        //! SDL_MetalView (opaque) — Metal mode only. SDL requires it be destroyed
        //! before its window, so it is owned here rather than left to
        //! SDL_DestroyWindow.
        void* metalView_;
        //! CAMetalLayer* owned by metalView_, cached so getMetalLayer() stays a
        //! plain accessor. Non-owning.
        void* metalLayer_;

        WindowDetails windowDetails_;
        WindowFlags windowFlags_;
        GraphicsAPI graphicsAPI_;
        std::unique_ptr<SDLKeyboardListener> keyboardListener_;
        std::unique_ptr<SDLMouseListener> mouseListener_;
        std::unique_ptr<SDLTouchListener> touchListener_;

#ifdef __ANDROID__
        //! Last-seen ANativeWindow identity; pollEvents() compares against
        //! this each call to detect surface teardown/recreation. See
        //! WindowFlags::surfaceLost.
        void* lastNativeWindow_ = nullptr;
#endif
        bool windowShouldClose_;
        bool ownsSubsystem_; //!< this instance holds a reference to the SDL init

        void handleWindowEvent(const SDL_Event* event);
        void initializeSDL();
    };

} // namespace wma

#endif // WMA_BACKENDS_SDL_WINDOW_MANAGER_HPP
