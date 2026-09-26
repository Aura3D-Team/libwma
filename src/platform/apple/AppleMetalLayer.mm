#include "wma/platform/apple/AppleMetalLayer.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_OSX

#include <AppKit/AppKit.h>
#include <QuartzCore/CAMetalLayer.h>

namespace wma
{
namespace apple
{

void *attachMetalLayerToNSWindow(void *nsWindow) noexcept
{
    if (!nsWindow)
        return nullptr;

    NSWindow *window = static_cast<NSWindow *>(nsWindow);
    NSView *contentView = [window contentView];
    if (!contentView)
        return nullptr;

    CAMetalLayer *metalLayer = [CAMetalLayer layer];
    if (!metalLayer)
        return nullptr;

    /*
     * Order matters: assigning .layer *before* setting wantsLayer is what
     * makes this a layer-hosting view, i.e. what makes AppKit adopt this
     * exact layer. Setting wantsLayer first would have AppKit create its own
     * backing layer and treat ours as a sublayer — which then never gets
     * sized or presented, and shows as a window that stays blank while the
     * renderer reports drawing perfectly normally.
     */
    contentView.layer = metalLayer;
    contentView.wantsLayer = YES;

    /*
     * Nothing else teaches a hand-made layer about the display's DPI (SDL's
     * own metal view does this internally, which is why the SDL path needs
     * no equivalent). Without it, a Retina window presents a drawable at
     * half the backing-store resolution, upscaled.
     */
    metalLayer.contentsScale = [window backingScaleFactor];

    return metalLayer;
}

} // namespace apple
} // namespace wma

#else

namespace wma
{
namespace apple
{

//! iOS reaches its layer through SDL's UIView-backed metal view instead;
//! there is no NSWindow to attach to. Defined rather than omitted so the
//! translation unit is never empty, which some archivers warn about.
void *attachMetalLayerToNSWindow(void *) noexcept
{
    return nullptr;
}

} // namespace apple
} // namespace wma

#endif // TARGET_OS_OSX
