#ifdef WMA_ENABLE_X11
#include "wma/backends/x11/X11KeyboardListener.hpp"
#include "wma/exceptions/WMAException.hpp"
#include "wma/input/keyboard/Keys.h"
#include "wma/input/keyboard/Utf8.hpp"

#include <array>
#include <clocale>
#include <string_view>
#include <vector>

namespace wma
{

X11KeyboardListener::X11KeyboardListener() : KeyboardListener(), display_(nullptr)
{
}

X11KeyboardListener::~X11KeyboardListener()
{
    if (inputContext_)
    {
        XDestroyIC(inputContext_);
        inputContext_ = nullptr;
    }
    if (inputMethod_)
    {
        XCloseIM(inputMethod_);
        inputMethod_ = nullptr;
    }
}

void X11KeyboardListener::initialize(Display *display)
{
    if (!display)
    {
        throw InputException("Invalid X11 Display pointer");
    }
    display_ = display;
}

void X11KeyboardListener::attachWindow(Window window)
{
    if (!display_ || window == 0)
        return;

    window_ = window;

    /*
     * XIM needs a locale set and the X locale modifiers initialised, both of
     * which are process-global. Doing it here rather than at library init
     * keeps the side effect on the path that actually needs an input method:
     * a program that never opens an X11 window never has its locale touched.
     *
     * setlocale() only runs while the program is still in the default "C"
     * locale, so an application that already chose one keeps it.
     */
    if (const char *current = std::setlocale(LC_CTYPE, nullptr); !current || std::string_view{current} == "C")
    {
        std::setlocale(LC_CTYPE, "");
    }

    if (!XSupportsLocale())
        return; //! No usable locale: lookup falls back to XLookupString.

    XSetLocaleModifiers("");

    inputMethod_ = XOpenIM(display_, nullptr, nullptr, nullptr);
    if (!inputMethod_)
        return; //! No input method running (common in minimal environments).

    /*
     * XIMPreeditNothing | XIMStatusNothing is the "root window" style: the
     * input method draws its own pre-edit and status UI rather than asking
     * this application to. Every other style requires the client to render
     * candidate lists itself, which a windowing library has no business doing
     * -- and this style is the one every IM is required to support.
     */
    inputContext_ = XCreateIC(inputMethod_, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, window_,
                              XNFocusWindow, window_, nullptr);

    if (!inputContext_)
    {
        XCloseIM(inputMethod_);
        inputMethod_ = nullptr;
        return;
    }

    /*
     * The method may need events this window is not yet selecting for (an
     * on-the-spot style wants KeyRelease, for instance). Merging rather than
     * replacing keeps whatever X11WindowManager already asked for.
     */
    long imEventMask = 0;
    if (XGetICValues(inputContext_, XNFilterEvents, &imEventMask, nullptr) == nullptr && imEventMask != 0)
    {
        XWindowAttributes attributes{};
        if (XGetWindowAttributes(display_, window_, &attributes))
            XSelectInput(display_, window_, attributes.your_event_mask | imEventMask);
    }

    XSetICFocus(inputContext_);
}

bool X11KeyboardListener::filterEvent(XEvent *event) const
{
    //! XFilterEvent must be offered every event when an IM is in use, not just
    //! key events: the method communicates through synthetic events of its
    //! own. True means the method consumed it and nothing else should see it.
    return event != nullptr && XFilterEvent(event, None) == True;
}

void X11KeyboardListener::lookupAndDispatchText(const XKeyEvent &xKeyEvent)
{
    //! Both lookup functions take a non-const event; X11's API predates const.
    XKeyEvent mutableEvent = xKeyEvent;

    std::array<char, 64> stackBuffer{};
    KeySym keySym = NoSymbol;
    Status status = 0;

    if (!inputContext_)
    {
        /*
         * No input method available. XLookupString returns Latin-1 rather than
         * UTF-8, so its output is widened one byte at a time: for ASCII the two
         * coincide, and for 0xA0-0xFF a Latin-1 byte *is* the codepoint.
         * Characters composed through a dead key are lost here, which is the
         * price of having no IM -- ordinary typing still works.
         */
        const int length = XLookupString(&mutableEvent, stackBuffer.data(), static_cast<int>(stackBuffer.size()) - 1,
                                         &keySym, nullptr);

        for (int i = 0; i < length; ++i)
            dispatchText(static_cast<Codepoint>(static_cast<u8>(stackBuffer[static_cast<usize>(i)])));

        return;
    }

    int length = Xutf8LookupString(inputContext_, &mutableEvent, stackBuffer.data(),
                                   static_cast<int>(stackBuffer.size()) - 1, &keySym, &status);

    if (status == XBufferOverflow)
    {
        //! A commit longer than the stack buffer, i.e. an IME phrase. `length`
        //! now holds the size required, so one exactly-sized retry suffices.
        std::vector<char> heapBuffer(static_cast<usize>(length) + 1u, '\0');
        length = Xutf8LookupString(inputContext_, &mutableEvent, heapBuffer.data(), length, &keySym, &status);

        if (length > 0 && (status == XLookupChars || status == XLookupBoth))
        {
            utf8::decode(std::string_view{heapBuffer.data(), static_cast<usize>(length)},
                         [this](Codepoint codepoint)
                         {
                             dispatchText(codepoint);
                         });
        }
        return;
    }

    //! Anything else means the keystroke produced no text: a modifier, or a
    //! keysym with no character such as F1 or Home.
    if (status != XLookupChars && status != XLookupBoth)
        return;

    if (length > 0)
    {
        utf8::decode(std::string_view{stackBuffer.data(), static_cast<usize>(length)},
                     [this](Codepoint codepoint)
                     {
                         dispatchText(codepoint);
                     });
    }
}

void X11KeyboardListener::handleKeyEvent(KeySym x11Key, const XKeyEvent &xKeyEvent)
{
    const Key mappedKey = mapX11Key(x11Key);

    if (xKeyEvent.type == KeyPress)
    {
        /*
         * X11 puts no repeat flag on the event. Auto-repeat arrives as a
         * KeyPress for a key that is already held, which the tracked key state
         * can answer directly -- so a repeat is precisely a press whose key was
         * already down. This relies on X11WindowManager having enabled
         * detectable auto-repeat, without which the server also synthesises a
         * KeyRelease before each repeat and every press would look fresh.
         */
        const bool repeat = isKeyDown(mappedKey);
        dispatchKeyPress(mappedKey, repeat);

        //! After the key event, so a text handler that inspects modifiers()
        //! sees the state this keystroke produced.
        lookupAndDispatchText(xKeyEvent);
    }
    else if (xKeyEvent.type == KeyRelease)
    {
        dispatchKeyRelease(mappedKey);
    }
}

} // namespace wma
#endif
