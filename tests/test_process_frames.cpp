// processFrames(): the managed loop for a caller that does not draw every iteration. Only iterations that drew
// may swap, or an OpenGL window would present a back buffer nothing was drawn into.

#include <cstdio>
#include <vector>

#include <wma/wma.hpp>

namespace
{

int g_failures = 0;

void check(bool condition, const char *description)
{
    std::printf("%s %s\n", condition ? "ok  " : "FAIL", description);
    if (!condition)
        ++g_failures;
}

class InertMouse final : public wma::MouseListener
{
    void updateCursorState() override
    {
    }
};

//! Closes itself after a fixed number of polls and records what the loop asked of it.
class ScriptedWindow final : public wma::IWindowManager
{
  public:
    explicit ScriptedWindow(int iterations) : iterations_(iterations)
    {
    }

    int polls = 0;
    int swaps = 0;
    std::vector<int> swappedAfterPoll;

    void createWindow(const char *) override
    {
    }
    void close() noexcept override
    {
        iterations_ = polls;
    }
    void pollEvents() override
    {
        ++polls;
    }
    void swapBuffers() override
    {
        ++swaps;
        swappedAfterPoll.push_back(polls);
    }
    void *getWindowInstance() override
    {
        return this;
    }
    wma::WindowFlags *getWindowFlags() noexcept override
    {
        return &flags_;
    }
    const wma::WindowDetails *getWindowDetails() noexcept override
    {
        return &details_;
    }
    const std::vector<const char *> getVulkanExtensions() const override
    {
        return {};
    }
    wma::KeyboardListener &getKeyboardListener() noexcept override
    {
        return keyboard_;
    }
    wma::MouseListener &getMouseListener() noexcept override
    {
        return mouse_;
    }
    bool shouldClose() const override
    {
        return polls >= iterations_;
    }
    wma::WindowBackend getBackendType() const override
    {
        return wma::WindowBackend::SDL3;
    }
    wma::GraphicsAPI getGraphicsAPI() const override
    {
        return wma::GraphicsAPI::OpenGL;
    }
    wma::WmaCode destroy() override
    {
        return wma::WmaCode::Ok;
    }

  private:
    int iterations_;
    wma::WindowFlags flags_;
    wma::WindowDetails details_;
    wma::KeyboardListener keyboard_;
    InertMouse mouse_;
};

} // namespace

int main()
{
    {
        ScriptedWindow window{6};
        int calls = 0;
        window.processFrames(
            [&calls, &window]
            {
                ++calls;
                return window.polls % 3 == 1; // Draws on the 1st and 4th iterations only.
            });
        check(calls == 6 && window.polls == 6, "processFrames() polls and calls the frame once per iteration");
        check((window.swappedAfterPoll == std::vector<int>{1, 4}), "and swaps only after the iterations that drew");
    }
    {
        ScriptedWindow window{4};
        int calls = 0;
        window.process(
            [&calls]
            {
                ++calls;
            });
        check(calls == 4 && window.swaps == 4, "process() still swaps after every iteration");
    }
    return g_failures == 0 ? 0 : 1;
}
