#include "wma/input/touch/TouchListener.hpp"

#include <algorithm>
#include <utility>

namespace wma
{

TouchListener::TouchListener()
{
    downActions_.emplace_back();
    moveActions_.emplace_back();
    upActions_.emplace_back();
}

TouchListener::~TouchListener() = default;

InputContextId TouchListener::createContext()
{
    const InputContextId context = contexts_.createContext();
    ensureContextCapacity(context);
    return context;
}

void TouchListener::setActiveContext(InputContextId context)
{
    contexts_.setActiveContext(context);
}

void TouchListener::pushContext(InputContextId context)
{
    contexts_.pushContext(context);
}

void TouchListener::popContext()
{
    contexts_.popContext();
}

InputContextId TouchListener::getActiveContext() const
{
    return contexts_.activeContext();
}

InputContextId TouchListener::getResolvedContext() const
{
    return contexts_.resolved();
}

void TouchListener::setDownAction(TouchInputCallback action)
{
    setDownAction(std::move(action), contexts_.resolved());
}

void TouchListener::setDownAction(TouchInputCallback action, InputContextId context)
{
    ensureContextCapacity(context);
    downActions_[context] = std::move(action);
}

void TouchListener::setMoveAction(TouchInputCallback action)
{
    setMoveAction(std::move(action), contexts_.resolved());
}

void TouchListener::setMoveAction(TouchInputCallback action, InputContextId context)
{
    ensureContextCapacity(context);
    moveActions_[context] = std::move(action);
}

void TouchListener::setUpAction(TouchInputCallback action)
{
    setUpAction(std::move(action), contexts_.resolved());
}

void TouchListener::setUpAction(TouchInputCallback action, InputContextId context)
{
    ensureContextCapacity(context);
    upActions_[context] = std::move(action);
}

void TouchListener::clearAllActions()
{
    std::ranges::for_each(downActions_,
                          [](auto &a)
                          {
                              a = TouchInputCallback{};
                          });
    std::ranges::for_each(moveActions_,
                          [](auto &a)
                          {
                              a = TouchInputCallback{};
                          });
    std::ranges::for_each(upActions_,
                          [](auto &a)
                          {
                              a = TouchInputCallback{};
                          });
}

void TouchListener::clearAllActions(InputContextId context)
{
    ensureContextCapacity(context);
    downActions_[context] = TouchInputCallback{};
    moveActions_[context] = TouchInputCallback{};
    upActions_[context] = TouchInputCallback{};
}

void TouchListener::dispatchDown(const WMATouchPoint &point)
{
    const InputContextId ctx = contexts_.resolved();
    if (ctx >= downActions_.size()) [[unlikely]]
        return;
    if (downActions_[ctx].valid())
        downActions_[ctx](point);
}

void TouchListener::dispatchMove(const WMATouchPoint &point)
{
    const InputContextId ctx = contexts_.resolved();
    if (ctx >= moveActions_.size()) [[unlikely]]
        return;
    if (moveActions_[ctx].valid())
        moveActions_[ctx](point);
}

void TouchListener::dispatchUp(const WMATouchPoint &point)
{
    const InputContextId ctx = contexts_.resolved();
    if (ctx >= upActions_.size()) [[unlikely]]
        return;
    if (upActions_[ctx].valid())
        upActions_[ctx](point);
}

void TouchListener::ensureContextCapacity(InputContextId context)
{
    const usize needed = static_cast<usize>(context) + 1;
    if (downActions_.size() < needed)
        downActions_.resize(needed);
    if (moveActions_.size() < needed)
        moveActions_.resize(needed);
    if (upActions_.size() < needed)
        upActions_.resize(needed);
}

} // namespace wma
