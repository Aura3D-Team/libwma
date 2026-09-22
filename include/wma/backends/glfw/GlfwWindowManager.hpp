#ifndef WMA_BACKENDS_GLFW_WINDOW_MANAGER_HPP
#define WMA_BACKENDS_GLFW_WINDOW_MANAGER_HPP

#include <memory>

#include "GLFWMouseListener.hpp"
#include "GLFWKeyboardListener.hpp"
#include "wma/managers/IWindowManager.hpp"

struct GLFWwindow;

namespace wma {

    class KeyboardListener;
    class MouseListener;

    struct GlfwUserData {
        class GlfwWindowManager* windowManager = nullptr;
        KeyboardListener* keyboardListener = nullptr;
        MouseListener* mouseListener = nullptr;
    };

    class GlfwWindowManager : public IWindowManager {
    public:
        explicit GlfwWindowManager(const WindowDetails& windowDetails,
                                   GraphicsAPI graphicsAPI = GraphicsAPI::Vulkan);
        ~GlfwWindowManager() override;

        GlfwWindowManager(const GlfwWindowManager&) = delete;
        GlfwWindowManager& operator=(const GlfwWindowManager&) = delete;
        GlfwWindowManager(GlfwWindowManager&&) noexcept;
        GlfwWindowManager& operator=(GlfwWindowManager&&) noexcept;

        void createWindow(const char* windowName) override;
        void pollEvents() override;
        void waitEvents(int timeoutMs) override;
        void swapBuffers() override;
        void* getWindowInstance() override;
        void* getGLProcAddress(const char* name) const override;
        [[nodiscard]] void* getMetalLayer() const noexcept override;
        WindowFlags* getWindowFlags() noexcept override;
        [[nodiscard]] FramebufferSize getFramebufferSize() noexcept override;
        const WindowDetails* getWindowDetails() noexcept override;
        const std::vector<const char*> getVulkanExtensions() const override;
        KeyboardListener& getKeyboardListener() noexcept override;
        void setTextInputEnabled(bool enabled) noexcept override;
        [[nodiscard]] bool isTextInputEnabled() const noexcept override;
        MouseListener& getMouseListener() noexcept override;
        bool shouldClose() const override;
        WindowBackend getBackendType() const override;
        GraphicsAPI getGraphicsAPI() const override;
        WmaCode destroy() override;

    private:
        GLFWwindow* window_;
        //! CAMetalLayer* attached to the window's content view — Metal mode only.
        //! Owned by that view (and so by the window), not by this object.
        void* metalLayer_;
        WindowDetails windowDetails_;
        WindowFlags windowFlags_;
        GraphicsAPI graphicsAPI_;
        std::unique_ptr<GLFWKeyboardListener> keyboardListener_;
        std::unique_ptr<GLFWMouseListener> mouseListener_;
        std::unique_ptr<GlfwUserData> userData_;
        bool windowShouldClose_;
        bool ownsInit_;  //!< this instance holds a reference to glfwInit()

        static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
        static void windowFocusCallback(GLFWwindow* window, int focused);
        static void windowIconifyCallback(GLFWwindow* window, int iconified);
        void initializeGLFW();
        static GlfwWindowManager* getInstanceFromWindow(GLFWwindow* window);
    };

} // namespace wma

#endif // WMA_BACKENDS_GLFW_WINDOW_MANAGER_HPP
