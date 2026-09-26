#ifndef WMA_INPUT_TOUCH_INPUT_CALLBACK_HPP
#define WMA_INPUT_TOUCH_INPUT_CALLBACK_HPP

#include <memory>

#include "wma/core/Types.hpp"
#include "wma/input/touch/TouchTypes.hpp"

namespace wma
{

struct TouchInputCallback
{
    using Fn = void (*)(void *, const WMATouchPoint &);

    Fn fn = nullptr;
    void *data = nullptr;
    std::shared_ptr<void> storage_;

    constexpr TouchInputCallback() = default;

    TouchInputCallback(Fn callback, void *userData) noexcept : fn(callback), data(userData)
    {
    }

    void operator()(const WMATouchPoint &point) const noexcept(false)
    {
        if (fn) [[likely]]
            fn(data, point);
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return fn != nullptr;
    }

    static TouchInputCallback from(wma::move_only_function<void(const WMATouchPoint &)> callback);
};

} // namespace wma

#endif // WMA_INPUT_TOUCH_INPUT_CALLBACK_HPP
