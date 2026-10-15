#include "backends/x11/X11Clipboard.hpp"

#include <X11/Xatom.h>
#include <poll.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>

#include "wma/input/keyboard/Utf8.hpp"

namespace wma
{
namespace
{

using Clock = std::chrono::steady_clock;

//! How long an owner gets to answer, which is the longest a paste can stall the caller.
constexpr auto kReplyTimeout = std::chrono::milliseconds(500);

//! A chunked transfer that keeps trickling is cut off here.
constexpr auto kTransferLimit = std::chrono::seconds(2);

//! A clipboard is text a person copied; past this it is refused rather than read into memory.
constexpr usize kMaxBytes = usize{16} << 20;

int ignoreError(Display *, XErrorEvent *)
{
    return 0;
}

//! The requestor may be gone before it is answered, and Xlib's default handler would end this process for the error.
class ErrorTrap
{
  public:
    explicit ErrorTrap(Display *display) : display_(display), previous_(XSetErrorHandler(ignoreError))
    {
    }
    ~ErrorTrap()
    {
        XSync(display_, False);
        XSetErrorHandler(previous_);
    }
    ErrorTrap(const ErrorTrap &) = delete;
    ErrorTrap &operator=(const ErrorTrap &) = delete;

  private:
    Display *display_;
    XErrorHandler previous_;
};

struct Wanted
{
    Window window;
    Atom property;
};

Bool isSelectionNotify(Display *, XEvent *event, XPointer argument)
{
    const auto &wanted = *reinterpret_cast<const Wanted *>(argument);
    return event->type == SelectionNotify && event->xselection.requestor == wanted.window;
}

//! Only this window's change to this property: unrelated property events stay queued for the event loop.
Bool isIncrementChunk(Display *, XEvent *event, XPointer argument)
{
    const auto &wanted = *reinterpret_cast<const Wanted *>(argument);
    return event->type == PropertyNotify && event->xproperty.window == wanted.window &&
           event->xproperty.atom == wanted.property && event->xproperty.state == PropertyNewValue;
}

//! Sleeps until the connection has something to read or @p deadline passes. False once the deadline has.
bool waitForServer(Display *display, Clock::time_point deadline)
{
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
    if (remaining <= 0)
        return false;

    pollfd ready{ConnectionNumber(display), POLLIN, 0};
    poll(&ready, 1, static_cast<int>(remaining));
    //! Moves what arrived from the socket into Xlib's queue, where the next check will find it.
    XPending(display);
    return true;
}

[[nodiscard]] std::string latin1ToUtf8(std::string_view text)
{
    std::string utf8;
    utf8.reserve(text.size());
    for (const char byte : text)
    {
        const auto value = static_cast<u8>(byte);
        if (value < 0x80u)
        {
            utf8.push_back(byte);
            continue;
        }
        utf8.push_back(static_cast<char>(0xC0u | (value >> 6)));
        utf8.push_back(static_cast<char>(0x80u | (value & 0x3Fu)));
    }
    return utf8;
}

//! What a client that only knows the STRING target can show; anything outside Latin-1 becomes '?'.
[[nodiscard]] std::string utf8ToLatin1(std::string_view text)
{
    std::string latin1;
    latin1.reserve(text.size());
    for (usize index = 0; index < text.size();)
    {
        const utf8::DecodeResult decoded = utf8::decodeOne(text, index);
        latin1.push_back(decoded.ok() && decoded.codepoint < 0x100u ? static_cast<char>(decoded.codepoint) : '?');
        index = std::max(decoded.next, index + 1);
    }
    return latin1;
}

struct XFreeDeleter
{
    void operator()(unsigned char *data) const noexcept
    {
        XFree(data);
    }
};

} // namespace

X11Clipboard::X11Clipboard(Display *display, Window window) : display_(display), window_(window)
{
    std::array<const char *, 8> names{"CLIPBOARD",  "TARGETS", "UTF8_STRING", "text/plain;charset=utf-8",
                                      "text/plain", "TEXT",    "INCR",        "WMA_CLIPBOARD"};
    std::array<Atom, names.size()> atoms{};
    XInternAtoms(display_, const_cast<char **>(names.data()), static_cast<int>(names.size()), False, atoms.data());
    clipboard_ = atoms[0];
    targets_ = atoms[1];
    utf8_ = atoms[2];
    textPlainUtf8_ = atoms[3];
    textPlain_ = atoms[4];
    textAtom_ = atoms[5];
    incr_ = atoms[6];
    property_ = atoms[7];
}

X11Clipboard::~X11Clipboard()
{
    //! Asked rather than trusted from owned_: a SelectionClear still queued means another client owns it now, and
    //! giving up the selection would take theirs.
    if (owned_ && XGetSelectionOwner(display_, clipboard_) == window_)
        XSetSelectionOwner(display_, clipboard_, None, CurrentTime);
}

bool X11Clipboard::setText(std::string_view text)
{
    ownedText_.assign(text);
    XSetSelectionOwner(display_, clipboard_, window_, CurrentTime);

    //! The server decides who owns it; asking is the only way to know this request won.
    owned_ = XGetSelectionOwner(display_, clipboard_) == window_;
    if (!owned_)
        ownedText_.clear();
    return owned_;
}

std::string X11Clipboard::text()
{
    //! Asked of the server rather than trusted from owned_: another client may have taken the selection a moment ago,
    //! and the SelectionClear that says so is not necessarily handled yet.
    const Window owner = XGetSelectionOwner(display_, clipboard_);
    if (owner == None)
        return {};
    if (owner == window_)
        return owned_ ? ownedText_ : std::string{};

    std::string text;
    for (const Atom target : {utf8_, static_cast<Atom>(XA_STRING)})
    {
        switch (convert(target, text))
        {
        case Reply::Done:
            return text;
        case Reply::TimedOut:
            return {};
        case Reply::Refused:
            break;
        }
    }
    return {};
}

X11Clipboard::Reply X11Clipboard::convert(Atom target, std::string &out)
{
    XDeleteProperty(display_, window_, property_);
    XConvertSelection(display_, clipboard_, target, property_, window_, CurrentTime);
    XFlush(display_);

    const Clock::time_point deadline = Clock::now() + kReplyTimeout;
    const Wanted wanted{window_, property_};
    XEvent reply;
    while (
        !XCheckIfEvent(display_, &reply, isSelectionNotify, reinterpret_cast<XPointer>(const_cast<Wanted *>(&wanted))))
    {
        if (!waitForServer(display_, deadline))
            return Reply::TimedOut;
    }

    if (reply.xselection.property == None)
        return Reply::Refused;

    std::string data;
    if (propertyType() == incr_)
    {
        /*
         * The owner starts sending when the announcement is deleted, so everything queued for this property before
         * that -- the announcement's own change notice above all -- is stale and has to go first. Done in this order
         * nothing can be thrown away that belongs to a chunk, because no chunk exists yet.
         */
        const Wanted wanted{window_, property_};
        XEvent stale;
        while (XCheckIfEvent(display_, &stale, isIncrementChunk,
                             reinterpret_cast<XPointer>(const_cast<Wanted *>(&wanted))))
        {
        }
        XDeleteProperty(display_, window_, property_);
        XFlush(display_);
        if (!readIncremental(data))
            return Reply::TimedOut;
    }
    else
    {
        Atom type = None;
        if (!takeProperty(data, type))
            return Reply::Refused;
    }

    out = target == XA_STRING ? latin1ToUtf8(data) : std::move(data);
    return Reply::Done;
}

Atom X11Clipboard::propertyType() const noexcept
{
    Atom type = None;
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char *raw = nullptr;

    //! Zero length: the type and size are reported, and nothing is read or removed.
    XGetWindowProperty(display_, window_, property_, 0, 0, False, AnyPropertyType, &type, &format, &count, &remaining,
                       &raw);
    if (raw)
        XFree(raw);
    return type;
}

bool X11Clipboard::readIncremental(std::string &out)
{
    const Wanted wanted{window_, property_};
    const Clock::time_point limit = Clock::now() + kTransferLimit;
    for (;;)
    {
        const Clock::time_point deadline = std::min(Clock::now() + kReplyTimeout, limit);
        XEvent chunkEvent;
        while (!XCheckIfEvent(display_, &chunkEvent, isIncrementChunk,
                              reinterpret_cast<XPointer>(const_cast<Wanted *>(&wanted))))
        {
            if (!waitForServer(display_, deadline))
                return false;
        }

        std::string chunk;
        Atom type = None;
        if (!takeProperty(chunk, type))
            return false;

        //! Reading the chunk deleted it, which is what asks the owner for the next; an empty one ends the transfer.
        if (chunk.empty())
            return true;
        if (out.size() + chunk.size() > kMaxBytes)
            return false;
        out += chunk;
    }
}

bool X11Clipboard::takeProperty(std::string &out, Atom &type)
{
    int format = 0;
    unsigned long count = 0;
    unsigned long remaining = 0;
    unsigned char *raw = nullptr;

    //! Lengths are in 32-bit units. Deleted as it is read, which is also the acknowledgement an INCR owner waits for.
    const int status = XGetWindowProperty(display_, window_, property_, 0, static_cast<long>(kMaxBytes / 4 + 1), True,
                                          AnyPropertyType, &type, &format, &count, &remaining, &raw);
    const std::unique_ptr<unsigned char, XFreeDeleter> data{raw};
    if (status != Success || type == None)
        return false;

    //! Left unread, the rest of a property over the limit would sit on the window; it is not text worth keeping.
    if (remaining > 0)
    {
        XDeleteProperty(display_, window_, property_);
        return false;
    }

    if (format != 8)
        return false;

    out.assign(reinterpret_cast<const char *>(data.get()), count);
    return true;
}

usize X11Clipboard::maxPropertyBytes() const noexcept
{
    const long extended = XExtendedMaxRequestSize(display_);
    const long units = extended > 0 ? extended : XMaxRequestSize(display_);
    return static_cast<usize>(units) * 4 - 1024;
}

bool X11Clipboard::answer(const XSelectionRequestEvent &request, Atom property)
{
    if (request.target == targets_)
    {
        //! Atoms are longs on the client side whatever the wire format, which is why this is not an array of u32.
        const std::array<Atom, 6> offered{targets_, utf8_, textPlainUtf8_, textPlain_, textAtom_, XA_STRING};
        XChangeProperty(display_, request.requestor, property, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char *>(offered.data()), static_cast<int>(offered.size()));
        return true;
    }

    const bool utf8Target = request.target == utf8_ || request.target == textPlainUtf8_ ||
                            request.target == textPlain_ || request.target == textAtom_;
    if (!utf8Target && request.target != XA_STRING)
        return false;

    //! Sent whole or not at all: INCR on the sending side is not offered, so a larger text is declined.
    if (ownedText_.size() > maxPropertyBytes())
        return false;

    if (request.target == XA_STRING)
    {
        const std::string latin1 = utf8ToLatin1(ownedText_);
        XChangeProperty(display_, request.requestor, property, XA_STRING, 8, PropModeReplace,
                        reinterpret_cast<const unsigned char *>(latin1.data()), static_cast<int>(latin1.size()));
        return true;
    }

    //! TEXT is whatever the owner prefers, which here is UTF-8; the others name their own type.
    const Atom type = request.target == textAtom_ ? utf8_ : request.target;
    XChangeProperty(display_, request.requestor, property, type, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char *>(ownedText_.data()), static_cast<int>(ownedText_.size()));
    return true;
}

void X11Clipboard::handleRequest(const XSelectionRequestEvent &request)
{
    XSelectionEvent reply{};
    reply.type = SelectionNotify;
    reply.display = request.display;
    reply.requestor = request.requestor;
    reply.selection = request.selection;
    reply.target = request.target;
    reply.property = None;
    reply.time = request.time;

    //! A client that predates properties-in-requests names none, and expects the target's own atom.
    const Atom property = request.property != None ? request.property : request.target;
    {
        const ErrorTrap trap(display_);
        if (owned_ && request.selection == clipboard_ && answer(request, property))
            reply.property = property;
        XSendEvent(display_, request.requestor, False, NoEventMask, reinterpret_cast<XEvent *>(&reply));
    }
    XFlush(display_);
}

void X11Clipboard::handleClear(const XSelectionClearEvent &clear) noexcept
{
    if (clear.selection != clipboard_)
        return;

    owned_ = false;
    ownedText_.clear();
}

} // namespace wma
