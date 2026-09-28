#ifndef WMA_INPUT_INPUT_CALLBACK_HPP
#define WMA_INPUT_INPUT_CALLBACK_HPP

#include <memory>

#include "wma/core/Types.hpp"

namespace wma
{

/**
 * @brief Fast callable: raw function pointer + void* userdata for the zero-cost path
 * (free functions, static methods). Capturing lambdas are heap-allocated via
 * std::move_only_function and kept alive by storage_. The callable is always
 * invoked through fn(data) regardless of which path was used, so the hot-path
 * cost is a single null-check + one indirect call.
 */
struct InputCallback
{
    using Fn = void (*)(void *);

    Fn fn = nullptr;
    void *data = nullptr;
    std::shared_ptr<void> storage_;

    constexpr InputCallback() = default;

    InputCallback(Fn callback, void *userData) noexcept : fn(callback), data(userData)
    {
    }

    void operator()() const noexcept(false)
    {
        if (fn) [[likely]]
            fn(data);
    }

    [[nodiscard]] bool valid() const noexcept
    {
        return fn != nullptr;
    }

    //! Wraps any callable (including move-only capturing lambdas) into the
    //! fast fn+data representation. Uses std::move_only_function to avoid
    //! requiring copyability of the callable.
    static InputCallback from(wma::move_only_function<void()> callback);
};

} // namespace wma

#endif // WMA_INPUT_INPUT_CALLBACK_HPP
