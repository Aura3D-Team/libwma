#include "wma/input/keyboard/KeyEventCallback.hpp"

namespace wma
{

namespace
{

void invokeKeyEventCallback(void *d, const WMAKeyEvent &event)
{
    using Holder = move_only_function<void(const WMAKeyEvent &)>;
    (*static_cast<Holder *>(d))(event);
}

} // namespace

KeyEventCallback KeyEventCallback::from(move_only_function<void(const WMAKeyEvent &)> callback)
{
    if (!callback)
        return {};

    using Holder = move_only_function<void(const WMAKeyEvent &)>;
    auto holder = std::make_shared<Holder>(std::move(callback));

    KeyEventCallback result;
    result.fn = invokeKeyEventCallback;
    result.data = holder.get();
    result.storage_ = std::move(holder);
    return result;
}

} // namespace wma
