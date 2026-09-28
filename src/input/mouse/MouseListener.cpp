#include "wma/input/mouse/MouseListener.hpp"

#include <algorithm>
#include <utility>

namespace wma
{

MouseListener::MouseListener() : currentPosition_{}, lastPosition_{}
{
    buttonBindings_.emplace_back();
    moveActions_.emplace_back();
    scrollActions_.emplace_back();
}

MouseListener::~MouseListener() = default;

InputContextId MouseListener::createContext()
{
    const InputContextId context = contexts_.createContext();
    ensureContextCapacity(context);
    return context;
}

void MouseListener::setActiveContext(InputContextId context)
{
    contexts_.setActiveContext(context);
}

void MouseListener::pushContext(InputContextId context)
{
    contexts_.pushContext(context);
}

void MouseListener::popContext()
{
    contexts_.popContext();
}

InputContextId MouseListener::getActiveContext() const
{
    return contexts_.activeContext();
}

InputContextId MouseListener::getResolvedContext() const
{
    return contexts_.resolved();
}

void MouseListener::addButtonAction(i32 button, MouseAction action)
{
    addButtonAction(button, std::move(action), contexts_.resolved());
}

void MouseListener::addButtonAction(i32 button, MouseAction action, InputContextId context)
{
    if (button < 0 || static_cast<usize>(button) >= MOUSE_BUTTON_COUNT) [[unlikely]]
        return;
    ensureContextCapacity(context);
    buttonBindings_[context][static_cast<usize>(button)] = std::move(action);
}

void MouseListener::removeButtonAction(i32 button)
{
    removeButtonAction(button, contexts_.resolved());
}

void MouseListener::removeButtonAction(i32 button, InputContextId context)
{
    if (button < 0 || static_cast<usize>(button) >= MOUSE_BUTTON_COUNT) [[unlikely]]
        return;
    ensureContextCapacity(context);
    buttonBindings_[context].clearSlot(static_cast<usize>(button));
}

void MouseListener::setMoveAction(MouseAction action)
{
    setMoveAction(std::move(action), contexts_.resolved());
}

void MouseListener::setMoveAction(MouseAction action, InputContextId context)
{
    ensureContextCapacity(context);
    moveActions_[context] = std::move(action);
}

void MouseListener::setScrollAction(MouseAction action)
{
    setScrollAction(std::move(action), contexts_.resolved());
}

void MouseListener::setScrollAction(MouseAction action, InputContextId context)
{
    ensureContextCapacity(context);
    scrollActions_[context] = std::move(action);
}

void MouseListener::clearAllActions()
{
    std::ranges::for_each(buttonBindings_,
                          [](auto &t)
                          {
                              t.clear();
                          });
    std::ranges::for_each(moveActions_,
                          [](auto &a)
                          {
                              a = MouseAction{};
                          });
    std::ranges::for_each(scrollActions_,
                          [](auto &a)
                          {
                              a = MouseAction{};
                          });
}

void MouseListener::clearAllActions(InputContextId context)
{
    ensureContextCapacity(context);
    buttonBindings_[context].clear();
    moveActions_[context] = MouseAction{};
    scrollActions_[context] = MouseAction{};
}

bool MouseListener::hasButtonAction(i32 button) const
{
    return hasButtonAction(button, contexts_.resolved());
}

bool MouseListener::hasButtonAction(i32 button, InputContextId context) const
{
    if (button < 0 || static_cast<usize>(button) >= MOUSE_BUTTON_COUNT) [[unlikely]]
        return false;
    if (context >= buttonBindings_.size())
        return false;
    return buttonBindings_[context].has(static_cast<usize>(button));
}

WMAMousePosition MouseListener::getCurrentPosition() const
{
    return currentPosition_;
}

void MouseListener::setCursorEnabled(bool enabled)
{
    if (cursorEnabled_ != enabled)
    {
        cursorEnabled_ = enabled;
        updateCursorState();
    }
}

bool MouseListener::isCursorEnabled() const
{
    return cursorEnabled_;
}

void MouseListener::setSensitivity(f64 sensitivity)
{
    sensitivity_ = sensitivity;
}

f64 MouseListener::getSensitivity() const
{
    return sensitivity_;
}

void MouseListener::processPendingEvents(const PendingEvent &event)
{
    switch (event.type)
    {
    case PendingEvent::WMAMove:
        dispatchMove(event.position);
        break;
    case PendingEvent::WMAScroll:
        dispatchScroll(event.scroll);
        break;
    case PendingEvent::WMAButtonPress:
        dispatchButtonPress(event.button);
        break;
    case PendingEvent::WMAButtonRelease:
        dispatchButtonRelease(event.button);
        break;
    case PendingEvent::WMANone:
        break;
    default:
        std::unreachable();
    }
}

void MouseListener::dispatchButtonPress(i32 button)
{
    if (button < 0 || static_cast<usize>(button) >= MOUSE_BUTTON_COUNT) [[unlikely]]
        return;
    [[assume(button >= 0)]];
    [[assume(static_cast<usize>(button) < MOUSE_BUTTON_COUNT)]];

    const InputContextId ctx = contexts_.resolved();
    if (ctx >= buttonBindings_.size()) [[unlikely]]
        return;
    [[assume(ctx < buttonBindings_.size())]];

    buttonBindings_[ctx][static_cast<usize>(button)].executePress();
}

void MouseListener::dispatchButtonRelease(i32 button)
{
    if (button < 0 || static_cast<usize>(button) >= MOUSE_BUTTON_COUNT) [[unlikely]]
        return;
    [[assume(button >= 0)]];
    [[assume(static_cast<usize>(button) < MOUSE_BUTTON_COUNT)]];

    const InputContextId ctx = contexts_.resolved();
    if (ctx >= buttonBindings_.size()) [[unlikely]]
        return;
    [[assume(ctx < buttonBindings_.size())]];

    buttonBindings_[ctx][static_cast<usize>(button)].executeRelease();
}

void MouseListener::dispatchMove(const WMAMousePosition &position)
{
    const InputContextId ctx = contexts_.resolved();
    if (ctx >= moveActions_.size()) [[unlikely]]
        return;
    moveActions_[ctx].executeMove(position);
}

WMAMouseScroll MouseListener::consumeScrollDelta() noexcept
{
    const WMAMouseScroll drained = accumulatedScroll_;
    accumulatedScroll_ = WMAMouseScroll{};
    return drained;
}

void MouseListener::dispatchScroll(const WMAMouseScroll &scroll)
{
    //! Accumulated before dispatch, and unconditionally: a poller must see the
    //! wheel whether or not anything has claimed the scroll action.
    accumulatedScroll_.xOffset += scroll.xOffset;
    accumulatedScroll_.yOffset += scroll.yOffset;

    const InputContextId ctx = contexts_.resolved();
    if (ctx >= scrollActions_.size()) [[unlikely]]
        return;
    scrollActions_[ctx].executeScroll(scroll);
}

void MouseListener::ensureContextCapacity(InputContextId context)
{
    while (buttonBindings_.size() <= context)
    {
        buttonBindings_.emplace_back();
        moveActions_.emplace_back();
        scrollActions_.emplace_back();
    }
}

} // namespace wma
