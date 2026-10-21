// clang-format off
// Must precede every Wayland header: it redirects libwayland's inline wrappers to the run-time loader.
#include "backends/wayland/WaylandApi.hpp"
// clang-format on
#include "backends/wayland/WaylandClipboard.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>

namespace wma
{
namespace
{

using Clock = std::chrono::steady_clock;

//! What the text is offered as, best first for a client that picks the first it knows.
constexpr std::array<const char *, 5> kOffered{"text/plain;charset=utf-8", "text/plain", "UTF8_STRING", "TEXT",
                                               "STRING"};

//! What is read, preferred first. All of them but the last two are UTF-8; STRING is Latin-1 by convention but is in
//! practice sent as UTF-8 by the clients that offer it, which is why it is last.
constexpr std::array<const char *, 5> kPreferred{"text/plain;charset=utf-8", "UTF8_STRING", "text/plain", "TEXT",
                                                 "STRING"};

//! How long another client may go without moving a byte before it is given up on.
constexpr auto kIdleTimeout = std::chrono::milliseconds(500);

//! A transfer that keeps trickling is cut off here: both directions run on the event thread.
constexpr auto kTransferLimit = std::chrono::seconds(2);

//! A clipboard is text a person copied; past this it is refused rather than read into memory.
constexpr usize kMaxBytes = usize{16} << 20;

/**
 * Writes all of @p text to @p fd, which the compositor handed over and the reader on the other end may close at any
 * time. A write to a closed pipe raises SIGPIPE, which ends the process unless it is handled; changing the process's
 * disposition is not a library's call, so it is blocked on this thread for the duration and any it raised is taken.
 */
void writeAll(int fd, std::string_view text)
{
    const int flags = fcntl(fd, F_GETFL);
    if (flags >= 0)
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    sigset_t pipeSignal;
    sigemptyset(&pipeSignal);
    sigaddset(&pipeSignal, SIGPIPE);
    sigset_t previous;
    pthread_sigmask(SIG_BLOCK, &pipeSignal, &previous);

    bool brokenPipe = false;
    const Clock::time_point limit = Clock::now() + kTransferLimit;
    Clock::time_point deadline = Clock::now() + kIdleTimeout;
    while (!text.empty() && Clock::now() < std::min(deadline, limit))
    {
        const ssize_t written = write(fd, text.data(), text.size());
        if (written > 0)
        {
            text.remove_prefix(static_cast<usize>(written));
            deadline = Clock::now() + kIdleTimeout;
            continue;
        }
        if (written < 0 && errno == EINTR)
            continue;
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            pollfd ready{fd, POLLOUT, 0};
            poll(&ready, 1, 50);
            continue;
        }
        brokenPipe = written < 0 && errno == EPIPE;
        break;
    }

    if (brokenPipe)
    {
        const timespec none{};
        sigtimedwait(&pipeSignal, nullptr, &none);
    }
    pthread_sigmask(SIG_SETMASK, &previous, nullptr);
}

} // namespace

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
//! Drag and drop is unused: its events are received and ignored, and the v3 ones do not exist at version 1.
const wl_data_device_listener WaylandClipboard::deviceListener_ = {.data_offer = handleDataOffer,
                                                                   .enter = handleEnter,
                                                                   .leave = handleLeave,
                                                                   .motion = handleMotion,
                                                                   .drop = handleDrop,
                                                                   .selection = handleSelection};
const wl_data_offer_listener WaylandClipboard::offerListener_ = {.offer = handleOfferMime};
const wl_data_source_listener WaylandClipboard::sourceListener_ = {
    .target = handleSourceTarget, .send = handleSourceSend, .cancelled = handleSourceCancelled};
#pragma GCC diagnostic pop

WaylandClipboard::WaylandClipboard(wl_display *display) : display_(display)
{
}

WaylandClipboard::~WaylandClipboard()
{
    detachSeat();
    if (manager_)
        wl_data_device_manager_destroy(manager_);
}

void WaylandClipboard::adoptManager(wl_data_device_manager *manager)
{
    if (manager_)
    {
        wl_data_device_manager_destroy(manager);
        return;
    }
    manager_ = manager;
    ensureDevice();
}

void WaylandClipboard::attachSeat(wl_seat *seat)
{
    seat_ = seat;
    ensureDevice();
}

void WaylandClipboard::ensureDevice()
{
    if (device_ || !manager_ || !seat_)
        return;

    device_ = wl_data_device_manager_get_data_device(manager_, seat_);
    if (device_)
        wl_data_device_add_listener(device_, &deviceListener_, this);
}

void WaylandClipboard::detachSeat()
{
    dropSource();
    selection_ = nullptr;
    pruneOffers();
    if (device_)
    {
        //! Version 1 has no release request; the proxy is simply freed.
        wl_data_device_destroy(device_);
        device_ = nullptr;
    }
    seat_ = nullptr;
}

void WaylandClipboard::dropSource()
{
    if (source_)
    {
        wl_data_source_destroy(source_);
        source_ = nullptr;
    }
    ownedText_.clear();
}

bool WaylandClipboard::supported() const noexcept
{
    return device_ != nullptr;
}

bool WaylandClipboard::setText(std::string_view text, u32 serial)
{
    if (!device_ || serial == 0)
        return false;

    wl_data_source *source = wl_data_device_manager_create_data_source(manager_);
    if (!source)
        return false;

    wl_data_source_add_listener(source, &sourceListener_, this);
    for (const char *mime : kOffered)
        wl_data_source_offer(source, mime);
    wl_data_device_set_selection(device_, source, serial);

    //! The compositor cancels the source this replaces; destroyed here, so what it sends then finds nobody.
    if (source_)
        wl_data_source_destroy(source_);
    source_ = source;
    ownedText_.assign(text);

    wl_display_flush(display_);
    return true;
}

std::string WaylandClipboard::text()
{
    //! Whoever else is asked, this client already has it.
    if (source_)
        return ownedText_;
    if (!selection_)
        return {};

    for (const char *wanted : kPreferred)
    {
        const bool offered = std::ranges::any_of(selection_->mimes,
                                                 [wanted](const std::string &mime)
                                                 {
                                                     return mime == wanted;
                                                 });
        if (offered)
            return receive(selection_->proxy, wanted);
    }
    return {};
}

std::string WaylandClipboard::receive(wl_data_offer *offer, const char *mime)
{
    std::array<int, 2> pipeEnds{};
    if (pipe2(pipeEnds.data(), O_CLOEXEC) != 0)
        return {};

    wl_data_offer_receive(offer, mime, pipeEnds[1]);
    close(pipeEnds[1]);
    //! The request has to reach the compositor before this waits on its answer.
    wl_display_flush(display_);

    std::string result;
    const Clock::time_point limit = Clock::now() + kTransferLimit;
    Clock::time_point deadline = Clock::now() + kIdleTimeout;
    bool complete = false;
    while (Clock::now() < std::min(deadline, limit))
    {
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::min(deadline, limit) - Clock::now()).count();
        pollfd ready{pipeEnds[0], POLLIN, 0};
        const int polled = poll(&ready, 1, static_cast<int>(std::max<long>(remaining, 1)));
        if (polled < 0 && errno != EINTR)
            break;
        if (polled <= 0)
            continue;

        std::array<char, 65536> buffer;
        const ssize_t count = read(pipeEnds[0], buffer.data(), buffer.size());
        if (count == 0)
        {
            complete = true;
            break;
        }
        if (count < 0)
        {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            break;
        }
        if (result.size() + static_cast<usize>(count) > kMaxBytes)
            break;
        result.append(buffer.data(), static_cast<usize>(count));
        deadline = Clock::now() + kIdleTimeout;
    }
    close(pipeEnds[0]);

    //! Half a text would be pasted as if it were all of it.
    return complete ? result : std::string{};
}

void WaylandClipboard::pruneOffers()
{
    std::erase_if(offers_,
                  [this](const std::unique_ptr<Offer> &offer)
                  {
                      if (offer.get() == selection_)
                          return false;
                      wl_data_offer_destroy(offer->proxy);
                      return true;
                  });
}

void WaylandClipboard::handleDataOffer(void *data, wl_data_device *, wl_data_offer *offer)
{
    auto &self = *static_cast<WaylandClipboard *>(data);
    auto &held = self.offers_.emplace_back(std::make_unique<Offer>());
    held->proxy = offer;
    wl_data_offer_add_listener(offer, &offerListener_, held.get());
}

void WaylandClipboard::handleOfferMime(void *data, wl_data_offer *, const char *mime)
{
    static_cast<Offer *>(data)->mimes.emplace_back(mime);
}

void WaylandClipboard::handleEnter(void *, wl_data_device *, u32, wl_surface *, wl_fixed_t, wl_fixed_t, wl_data_offer *)
{
}

void WaylandClipboard::handleLeave(void *data, wl_data_device *)
{
    static_cast<WaylandClipboard *>(data)->pruneOffers();
}

void WaylandClipboard::handleMotion(void *, wl_data_device *, u32, wl_fixed_t, wl_fixed_t)
{
}

void WaylandClipboard::handleDrop(void *data, wl_data_device *)
{
    static_cast<WaylandClipboard *>(data)->pruneOffers();
}

void WaylandClipboard::handleSelection(void *data, wl_data_device *, wl_data_offer *offer)
{
    auto &self = *static_cast<WaylandClipboard *>(data);
    self.selection_ = nullptr;
    for (const std::unique_ptr<Offer> &held : self.offers_)
    {
        if (offer && held->proxy == offer)
            self.selection_ = held.get();
    }
    self.pruneOffers();
}

void WaylandClipboard::handleSourceTarget(void *, wl_data_source *, const char *)
{
}

void WaylandClipboard::handleSourceSend(void *data, wl_data_source *source, const char *mime, i32 fd)
{
    auto &self = *static_cast<WaylandClipboard *>(data);
    const bool known = std::ranges::any_of(kOffered,
                                           [mime](const char *offered)
                                           {
                                               return std::string_view{offered} == mime;
                                           });
    if (source == self.source_ && known)
        writeAll(fd, self.ownedText_);
    close(fd);
}

void WaylandClipboard::handleSourceCancelled(void *data, wl_data_source *source)
{
    auto &self = *static_cast<WaylandClipboard *>(data);
    if (source != self.source_)
        return;

    //! Another client took the selection.
    self.dropSource();
}

} // namespace wma
