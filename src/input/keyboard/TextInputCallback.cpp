#include "wma/input/keyboard/TextInputCallback.hpp"

namespace wma
{

namespace
{

void invokeTextInputCallback(void *d, Codepoint codepoint)
{
    using Holder = move_only_function<void(Codepoint)>;
    (*static_cast<Holder *>(d))(codepoint);
}

} // namespace

TextInputCallback TextInputCallback::from(move_only_function<void(Codepoint)> callback)
{
    if (!callback)
        return {};

    using Holder = move_only_function<void(Codepoint)>;
    auto holder = std::make_shared<Holder>(std::move(callback));

    TextInputCallback result;
    result.fn = invokeTextInputCallback;
    result.data = holder.get();
    result.storage_ = std::move(holder);
    return result;
}

} // namespace wma
