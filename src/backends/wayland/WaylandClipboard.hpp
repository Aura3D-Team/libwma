#ifndef WMA_BACKENDS_WAYLAND_CLIPBOARD_HPP
#define WMA_BACKENDS_WAYLAND_CLIPBOARD_HPP

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <wayland-client.h>

#include <ink/ink_base.hpp>

namespace wma
{

/**
 * @class WaylandClipboard
 * @brief The seat's selection over wl_data_device: setting it, and reading what another client set.
 *
 * Wayland keeps the data in the client that owns it and passes it through a pipe when another client asks, so this
 * object answers for the text it set (as a wl_data_source) for as long as it is the selection, and reads the current
 * one through the wl_data_offer the compositor announces. A selection is only accepted with the serial of a recent
 * input event of the seat, so setText() takes one from its caller.
 *
 * Bound at version 1 of wl_data_device_manager, which carries all that text needs; drag and drop is not used.
 */
class WaylandClipboard
{
  public:
    explicit WaylandClipboard(wl_display *display);
    ~WaylandClipboard();

    WaylandClipboard(const WaylandClipboard &) = delete;
    WaylandClipboard &operator=(const WaylandClipboard &) = delete;

    //! From the registry. Globals arrive in any order, so a device is made once both it and a seat are known.
    void adoptManager(wl_data_device_manager *manager);
    void attachSeat(wl_seat *seat);
    //! The seat went away: its device goes with it, and so does a selection this object held.
    void detachSeat();

    [[nodiscard]] bool supported() const noexcept;

    //! False without a device or a serial; true means the request was made, which the compositor may still decline.
    bool setText(std::string_view text, u32 serial);

    //! Empty when there is no selection, none in a text format, or its owner did not answer in time.
    [[nodiscard]] std::string text();

  private:
    struct Offer
    {
        wl_data_offer *proxy = nullptr;
        std::vector<std::string> mimes;
    };

    void ensureDevice();
    //! Offers other than the current selection are drag-and-drop ones this client never accepts.
    void pruneOffers();
    void dropSource();
    [[nodiscard]] std::string receive(wl_data_offer *offer, const char *mime);

    static void handleDataOffer(void *data, wl_data_device *device, wl_data_offer *offer);
    static void handleEnter(void *data, wl_data_device *device, u32 serial, wl_surface *surface, wl_fixed_t x,
                            wl_fixed_t y, wl_data_offer *offer);
    static void handleLeave(void *data, wl_data_device *device);
    static void handleMotion(void *data, wl_data_device *device, u32 time, wl_fixed_t x, wl_fixed_t y);
    static void handleDrop(void *data, wl_data_device *device);
    static void handleSelection(void *data, wl_data_device *device, wl_data_offer *offer);
    static void handleOfferMime(void *data, wl_data_offer *offer, const char *mime);
    static void handleSourceTarget(void *data, wl_data_source *source, const char *mime);
    static void handleSourceSend(void *data, wl_data_source *source, const char *mime, i32 fd);
    static void handleSourceCancelled(void *data, wl_data_source *source);

    static const wl_data_device_listener deviceListener_;
    static const wl_data_offer_listener offerListener_;
    static const wl_data_source_listener sourceListener_;

    wl_display *display_;
    wl_data_device_manager *manager_ = nullptr;
    wl_seat *seat_ = nullptr;
    wl_data_device *device_ = nullptr;

    std::vector<std::unique_ptr<Offer>> offers_;
    Offer *selection_ = nullptr;

    //! Non-null while this client is the selection's owner.
    wl_data_source *source_ = nullptr;
    std::string ownedText_;
};

} // namespace wma

#endif // WMA_BACKENDS_WAYLAND_CLIPBOARD_HPP
