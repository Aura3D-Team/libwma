#include "wma/input/touch/TouchInputCallback.hpp"

namespace wma
{

namespace
{

void invokeTouchCallback(void *d, const WMATouchPoint &point)
{
    using Holder = move_only_function<void(const WMATouchPoint &)>;
    (*static_cast<Holder *>(d))(point);
}

} // namespace

TouchInputCallback TouchInputCallback::from(move_only_function<void(const WMATouchPoint &)> callback)
{
    if (!callback)
        return {};

    using Holder = move_only_function<void(const WMATouchPoint &)>;
    auto holder = std::make_shared<Holder>(std::move(callback));

    TouchInputCallback result;
    result.fn = invokeTouchCallback;
    result.data = holder.get();
    result.storage_ = std::move(holder);
    return result;
}

} // namespace wma
