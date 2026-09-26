#include "wma/input/mouse/MouseInputCallback.hpp"

namespace wma
{

namespace
{

void invokeMoveCallback(void *d, const WMAMousePosition &pos)
{
    using Holder = move_only_function<void(const WMAMousePosition &)>;
    (*static_cast<Holder *>(d))(pos);
}

void invokeScrollCallback(void *d, const WMAMouseScroll &scroll)
{
    using Holder = move_only_function<void(const WMAMouseScroll &)>;
    (*static_cast<Holder *>(d))(scroll);
}

} // namespace

MoveInputCallback MoveInputCallback::from(move_only_function<void(const WMAMousePosition &)> callback)
{
    if (!callback)
        return {};

    using Holder = move_only_function<void(const WMAMousePosition &)>;
    auto holder = std::make_shared<Holder>(std::move(callback));

    MoveInputCallback result;
    result.fn = invokeMoveCallback;
    result.data = holder.get();
    result.storage_ = std::move(holder);
    return result;
}

ScrollInputCallback ScrollInputCallback::from(move_only_function<void(const WMAMouseScroll &)> callback)
{
    if (!callback)
        return {};

    using Holder = move_only_function<void(const WMAMouseScroll &)>;
    auto holder = std::make_shared<Holder>(std::move(callback));

    ScrollInputCallback result;
    result.fn = invokeScrollCallback;
    result.data = holder.get();
    result.storage_ = std::move(holder);
    return result;
}

} // namespace wma
