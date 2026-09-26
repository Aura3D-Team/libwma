#ifndef WMA_INPUT_MOUSE_INPUT_CALLBACK_HPP
#define WMA_INPUT_MOUSE_INPUT_CALLBACK_HPP

#include <memory>

#include "wma/core/Types.hpp"
#include "wma/input/mouse/MouseTypes.hpp"

namespace wma
{

struct MoveInputCallback
{
    using Fn = void (*)(void *, const WMAMousePosition &);

    Fn fn = nullptr;
    void *data = nullptr;
    std::shared_ptr<void> storage_;

    constexpr MoveInputCallback() = default;

    MoveInputCallback(Fn callback, void *userData) noexcept : fn(callback), data(userData)
    {
    }

    void operator()(const WMAMousePosition &position) const noexcept(false)
    {
        if (fn) [[likely]]
            fn(data, position);
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return fn != nullptr;
    }

    static MoveInputCallback from(wma::move_only_function<void(const WMAMousePosition &)> callback);
};

struct ScrollInputCallback
{
    using Fn = void (*)(void *, const WMAMouseScroll &);

    Fn fn = nullptr;
    void *data = nullptr;
    std::shared_ptr<void> storage_;

    constexpr ScrollInputCallback() = default;

    ScrollInputCallback(Fn callback, void *userData) noexcept : fn(callback), data(userData)
    {
    }

    void operator()(const WMAMouseScroll &scroll) const noexcept(false)
    {
        if (fn) [[likely]]
            fn(data, scroll);
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return fn != nullptr;
    }

    static ScrollInputCallback from(wma::move_only_function<void(const WMAMouseScroll &)> callback);
};

} // namespace wma

#endif // WMA_INPUT_MOUSE_INPUT_CALLBACK_HPP
