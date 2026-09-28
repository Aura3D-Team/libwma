#include "wma/input/InputCallback.hpp"

namespace wma
{

namespace
{

//! Typed invoker: called with a void* that is really a
//! std::move_only_function<void()>* kept alive through InputCallback::storage_.
void invokeVoidMoveOnly(void *d)
{
    (*static_cast<move_only_function<void()> *>(d))();
}

} // namespace

InputCallback InputCallback::from(move_only_function<void()> callback)
{
    if (!callback)
        return {};

    using Holder = move_only_function<void()>;
    auto holder = std::make_shared<Holder>(std::move(callback));

    InputCallback result;
    result.fn = invokeVoidMoveOnly;
    result.data = holder.get();
    result.storage_ = std::move(holder);
    return result;
}

} // namespace wma
