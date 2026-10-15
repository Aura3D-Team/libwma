/*
 * The X11 backend's clipboard: text set here is served to other clients, text another client set is read here, and
 * ownership changes hands in both directions. xclip stands in for "another application"; the checks that need it are
 * left out, with a note, when it is not installed. Runs against $WMA_TEST_DISPLAY, a disposable server (Xvfb);
 * skipped without one.
 */

#include <poll.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
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

//! The checks against another application launch xclip with fixed command lines; there is no user input in them.
bool haveXclip()
{
    // NOLINTNEXTLINE(bugprone-command-processor)
    return std::system("command -v xclip >/dev/null 2>&1") == 0;
}

/**
 * Another application reading the clipboard. This window has to keep servicing events while it waits, because it is
 * the one being asked; the reader's output is collected until it ends.
 */
std::string xclipRead(wma::IWindowManager &window, const char *target)
{
    const std::string command = std::string("xclip -selection clipboard -o -t ") + target + " 2>/dev/null";
    // NOLINTNEXTLINE(bugprone-command-processor)
    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
        return "<popen failed>";

    std::string output;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    const int fd = fileno(pipe);
    for (bool open = true; open && std::chrono::steady_clock::now() < deadline;)
    {
        window.waitEvents(5);
        pollfd ready{fd, POLLIN, 0};
        while (poll(&ready, 1, 0) > 0)
        {
            char buffer[65536];
            const ssize_t count = read(fd, buffer, sizeof buffer);
            if (count <= 0)
            {
                open = false;
                break;
            }
            output.append(buffer, static_cast<size_t>(count));
        }
    }
    pclose(pipe);
    return output;
}

//! Another application taking the clipboard. xclip keeps serving it from a background process, which must not hold
//! this test's own output open: the harness running it would wait for that process to end.
void xclipWrite(wma::IWindowManager &window, const std::string &text)
{
    // NOLINTNEXTLINE(bugprone-command-processor)
    FILE *pipe = popen("xclip -selection clipboard -i >/dev/null 2>&1", "w");
    if (!pipe)
        return;
    fwrite(text.data(), 1, text.size(), pipe);
    pclose(pipe);
    for (int i = 0; i < 20; ++i)
        window.waitEvents(5);
}

} // namespace

int main()
{
    // Never $DISPLAY: this opens windows and takes selections, which is not for a live desktop.
    const char *testDisplay = std::getenv("WMA_TEST_DISPLAY");
    if (!testDisplay)
    {
        std::puts("WMA_TEST_DISPLAY unset; skipped");
        return kSkipped;
    }
    setenv("DISPLAY", testDisplay, 1);

    try
    {
        wma::WindowDetails details;
        details.width = 320;
        details.height = 200;
        auto window = wma::createWindowManager(wma::WindowBackend::X11, details, wma::GraphicsAPI::CPU);
        window->createWindow("wma x11 clipboard");
        for (int i = 0; i < 10; ++i)
            window->waitEvents(5);

        check(window->isClipboardSupported(), "the X11 backend has a clipboard");

        const std::string ours = "caf\xC3\xA9 \xE2\x9C\x93 text";
        check(window->setClipboardText(ours), "setting the clipboard succeeds");
        check(window->getClipboardText() == ours, "and reads back as set");

        if (!haveXclip())
        {
            std::puts("xclip is not installed: the checks against another client are skipped");
            return failures == 0 ? 0 : 1;
        }

        check(xclipRead(*window, "UTF8_STRING") == ours, "another client reads it as UTF8_STRING, byte for byte");
        check(xclipRead(*window, "STRING") == "caf\xE9 ? text",
              "a client that only knows STRING gets Latin-1, with '?' for what does not fit");
        check(xclipRead(*window, "TARGETS").find("UTF8_STRING") != std::string::npos,
              "TARGETS lists the text formats on offer");
        check(xclipRead(*window, "image/png").empty(), "a target the window cannot supply is refused, not answered");

        xclipWrite(*window, "from another client \xE2\x9C\x93");
        check(window->getClipboardText() == "from another client \xE2\x9C\x93",
              "text another client set is read, even before the loss of ownership has been handled");

        check(window->setClipboardText("ours again"), "taking the clipboard back succeeds");
        check(xclipRead(*window, "UTF8_STRING") == "ours again", "and the other client then reads the new text");

        const std::string large(1 << 20, 'x');
        check(window->setClipboardText(large) && xclipRead(*window, "UTF8_STRING") == large,
              "a megabyte is served in one reply");

        xclipWrite(*window, large);
        const std::string megabyte = window->getClipboardText();
        std::printf("  received %zu of %zu bytes\n", megabyte.size(), large.size());
        check(megabyte == large, "and a megabyte from another client is read");

        //! Just past what one property can carry, so the owner sends it in chunks (INCR).
        const std::string chunked((16u << 20) - 20, 'y');
        xclipWrite(*window, chunked);
        const std::string received = window->getClipboardText();
        std::printf("  received %zu of %zu bytes\n", received.size(), chunked.size());
        check(received == chunked, "text sent in chunks is reassembled");

        xclipWrite(*window, std::string(20u << 20, 'z'));
        check(window->getClipboardText().empty(), "text over the size limit is refused rather than read into memory");

        check(window->setClipboardText("") && window->getClipboardText().empty(),
              "an empty clipboard reads back as empty");
    }
    catch (const std::exception &error)
    {
        std::printf("[FAIL] exception: %s\n", error.what());
        return 1;
    }

    return failures == 0 ? 0 : 1;
}
