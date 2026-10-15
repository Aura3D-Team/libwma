#ifndef WMA_BACKENDS_X11_CLIPBOARD_HPP
#define WMA_BACKENDS_X11_CLIPBOARD_HPP

#include <X11/Xlib.h>
#include <string>
#include <string_view>

#include <ink/ink_base.hpp>

namespace wma
{

/**
 * @class X11Clipboard
 * @brief The CLIPBOARD selection of one X11 window: owning it, answering for it, and reading it.
 *
 * X11 has no clipboard store. The owner holds the data and hands it to each requester when asked, so the window that
 * set the text must keep servicing events for as long as it wants the text to stay available: handleRequest() is how
 * it answers, and X11WindowManager::pollEvents() calls it.
 */
class X11Clipboard
{
  public:
    X11Clipboard(Display *display, Window window);
    ~X11Clipboard();

    X11Clipboard(const X11Clipboard &) = delete;
    X11Clipboard &operator=(const X11Clipboard &) = delete;

    //! Takes ownership of the selection. False when the server did not grant it.
    bool setText(std::string_view text);

    //! The selection's text, empty when there is none or its owner does not answer in time. Waits for the owner, but
    //! only takes the events that are its reply off the queue, so nothing else is lost or reordered.
    [[nodiscard]] std::string text();

    void handleRequest(const XSelectionRequestEvent &request);
    void handleClear(const XSelectionClearEvent &clear) noexcept;

  private:
    enum class Reply : u8
    {
        Done,
        Refused,  //!< The owner cannot supply this target; another may do.
        TimedOut, //!< It did not answer at all, so asking again would only wait again.
    };

    //! One ConvertSelection round for @p target, into @p out.
    [[nodiscard]] Reply convert(Atom target, std::string &out);
    //! The owner sends large data in chunks, each announced by a property change.
    [[nodiscard]] bool readIncremental(std::string &out);
    //! The type of the property without reading or deleting it; None when there is none.
    [[nodiscard]] Atom propertyType() const noexcept;
    //! The property's bytes and type, removed from the window. False when it is absent, not bytes, or cut short.
    [[nodiscard]] bool takeProperty(std::string &out, Atom &type);
    //! Answers one target for @p request, as @p property on the requestor.
    [[nodiscard]] bool answer(const XSelectionRequestEvent &request, Atom property);
    [[nodiscard]] usize maxPropertyBytes() const noexcept;

    Display *display_;
    Window window_;
    bool owned_ = false;
    std::string ownedText_;

    Atom clipboard_ = None;
    Atom targets_ = None;
    Atom utf8_ = None;
    Atom textPlainUtf8_ = None;
    Atom textPlain_ = None;
    Atom textAtom_ = None;
    Atom incr_ = None;
    Atom property_ = None;
};

} // namespace wma

#endif // WMA_BACKENDS_X11_CLIPBOARD_HPP
