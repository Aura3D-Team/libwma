#ifndef WMA_CORE_WINDOW_DETAILS_HPP
#define WMA_CORE_WINDOW_DETAILS_HPP

#include <ink/ink_base.hpp>

namespace wma
{

/**
 * @brief Configuration structure for window creation.
 *
 * An aggregate, so it supports both designated initialization
 * (`WindowDetails{ .width = 1920, .targetFPS = 144 }`) and the positional
 * form (`WindowDetails(1280, 720, true, 60)`) — the latter via C++20
 * parenthesized aggregate initialization. Any omitted field keeps its default.
 */
struct WindowDetails
{
    i32 width = 800;
    i32 height = 600;
    bool resizable = true;
    i32 targetFPS = 60;
    bool vsync = false;
    bool fullscreen = false;
};

} // namespace wma

#endif // WMA_CORE_WINDOW_DETAILS_HPP
