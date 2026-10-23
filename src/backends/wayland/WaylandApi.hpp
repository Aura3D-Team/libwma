#ifndef WMA_BACKENDS_WAYLAND_API_HPP
#define WMA_BACKENDS_WAYLAND_API_HPP

//! Include this before anything that includes <wayland-client.h>; WaylandLoader.hpp has the rest of the story.
//!
//! The stock header defines inline wrappers (wl_surface_commit, ...) in the global namespace that call
//! wl_proxy_marshal_flags and name the core wl_*_interface tables. Both are redirected before it is parsed: the
//! calls through the macros below, the tables through the generated rename header (our own copies of wayland.xml's
//! interfaces, which libwayland accepts because it compares interfaces by name).

#ifdef WAYLAND_CLIENT_PROTOCOL_H
#error "WaylandApi.hpp must precede every Wayland header in the translation unit"
#endif

#include <wayland-client-core.h>

#include "wma_wayland_core_rename.h"

namespace wma
{

inline namespace wayland_dyn
{
#define WMA_WL_CLIENT
// NOLINTNEXTLINE(bugprone-macro-parentheses): the argument names a declarator
#define WMA_WL_SYM(name) extern decltype(&::name) name;
#include "backends/wayland/WaylandSymbols.inc"
#undef WMA_WL_SYM
#undef WMA_WL_CLIENT
} // namespace wayland_dyn

} // namespace wma

#define wl_display_cancel_read (::wma::wayland_dyn::wl_display_cancel_read)
#define wl_display_connect (::wma::wayland_dyn::wl_display_connect)
#define wl_display_create_queue (::wma::wayland_dyn::wl_display_create_queue)
#define wl_display_disconnect (::wma::wayland_dyn::wl_display_disconnect)
#define wl_display_dispatch (::wma::wayland_dyn::wl_display_dispatch)
#define wl_display_dispatch_pending (::wma::wayland_dyn::wl_display_dispatch_pending)
#define wl_display_dispatch_queue_pending (::wma::wayland_dyn::wl_display_dispatch_queue_pending)
#define wl_display_flush (::wma::wayland_dyn::wl_display_flush)
#define wl_display_get_fd (::wma::wayland_dyn::wl_display_get_fd)
#define wl_display_prepare_read (::wma::wayland_dyn::wl_display_prepare_read)
#define wl_display_read_events (::wma::wayland_dyn::wl_display_read_events)
#define wl_display_roundtrip (::wma::wayland_dyn::wl_display_roundtrip)
#define wl_display_roundtrip_queue (::wma::wayland_dyn::wl_display_roundtrip_queue)
#define wl_event_queue_destroy (::wma::wayland_dyn::wl_event_queue_destroy)
#define wl_proxy_add_listener (::wma::wayland_dyn::wl_proxy_add_listener)
#define wl_proxy_create_wrapper (::wma::wayland_dyn::wl_proxy_create_wrapper)
#define wl_proxy_destroy (::wma::wayland_dyn::wl_proxy_destroy)
#define wl_proxy_get_user_data (::wma::wayland_dyn::wl_proxy_get_user_data)
#define wl_proxy_get_version (::wma::wayland_dyn::wl_proxy_get_version)
#define wl_proxy_marshal_flags (::wma::wayland_dyn::wl_proxy_marshal_flags)
#define wl_proxy_set_queue (::wma::wayland_dyn::wl_proxy_set_queue)
#define wl_proxy_set_user_data (::wma::wayland_dyn::wl_proxy_set_user_data)
#define wl_proxy_wrapper_destroy (::wma::wayland_dyn::wl_proxy_wrapper_destroy)

#include "backends/wayland/WaylandLoader.hpp"

#endif // WMA_BACKENDS_WAYLAND_API_HPP
