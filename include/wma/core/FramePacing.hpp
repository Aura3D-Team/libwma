#ifndef WMA_CORE_FRAME_PACING_HPP
#define WMA_CORE_FRAME_PACING_HPP

#include <ink/ink_base.hpp>

#include "Types.hpp"
#include "WindowDetails.hpp"

namespace wma::detail
{

//! The cap where the display's rate is unknown: not synchronisation, so a faster display still gets 60.
inline constexpr u32 kUnpacedVsyncFPS = 60;

/**
 * The rate process() holds the loop to. An explicit targetFPS always wins; vsync on a CPU window whose present
 * does not block would otherwise leave the loop uncapped.
 */
[[nodiscard]] constexpr u32 effectiveTargetFPS(const WindowDetails &details, GraphicsAPI api,
                                               bool presentWaitsForDisplay, u32 displayRefreshHz = 0) noexcept
{
    if (details.targetFPS > 0)
        return static_cast<u32>(details.targetFPS);
    if (details.vsync && api == GraphicsAPI::CPU && !presentWaitsForDisplay)
        return displayRefreshHz > 0 ? displayRefreshHz : kUnpacedVsyncFPS;
    return 0;
}

} // namespace wma::detail

#endif // WMA_CORE_FRAME_PACING_HPP
