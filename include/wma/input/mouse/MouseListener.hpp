#ifndef WMA_INPUT_MOUSE_LISTENER_HPP
#define WMA_INPUT_MOUSE_LISTENER_HPP

#include <vector>

#include "MouseAction.hpp"
#include "wma/input/InputBindingTable.hpp"
#include "wma/input/InputContextStack.hpp"
#include "wma/input/InputTypes.hpp"

namespace wma
{

struct PendingEvent
{
    enum WMAType
    {
        WMANone = 0,
        WMAMove,
        WMAScroll,
        WMAButtonPress,
        WMAButtonRelease
    } type = WMANone;

    WMAMousePosition position{};
    WMAMouseScroll scroll{};
    i32 button = -1;

    constexpr PendingEvent() = default;
    constexpr explicit PendingEvent(WMAType t) : type(t)
    {
    }
    constexpr PendingEvent(WMAType t, const WMAMousePosition &pos) : type(t), position(pos)
    {
    }
    constexpr PendingEvent(WMAType t, const WMAMouseScroll &s) : type(t), scroll(s)
    {
    }
    constexpr PendingEvent(WMAType t, i32 btn) : type(t), button(btn)
    {
    }
};

class MouseListener
{
  public:
    MouseListener();
    virtual ~MouseListener();

    MouseListener(const MouseListener &) = delete;
    MouseListener &operator=(const MouseListener &) = delete;
    MouseListener(MouseListener &&) = default;
    MouseListener &operator=(MouseListener &&) = default;

    //! Context management.
    [[nodiscard]] InputContextId createContext();
    void setActiveContext(InputContextId context);
    void pushContext(InputContextId context);
    void popContext();
    [[nodiscard]] InputContextId getActiveContext() const;
    [[nodiscard]] InputContextId getResolvedContext() const;

    //! Binding.
    void addButtonAction(i32 button, MouseAction action);
    void addButtonAction(i32 button, MouseAction action, InputContextId context);
    void removeButtonAction(i32 button);
    void removeButtonAction(i32 button, InputContextId context);

    void setMoveAction(MouseAction action);
    void setMoveAction(MouseAction action, InputContextId context);
    void setScrollAction(MouseAction action);
    void setScrollAction(MouseAction action, InputContextId context);

    void clearAllActions();
    void clearAllActions(InputContextId context);

    [[nodiscard]] bool hasButtonAction(i32 button) const;
    [[nodiscard]] bool hasButtonAction(i32 button, InputContextId context) const;

    [[nodiscard]] WMAMousePosition getCurrentPosition() const;

    /**
     * @brief Returns the wheel movement accumulated since the last call, and
     *        resets the accumulator.
     *
     * The polling counterpart to setScrollAction(), and the reason it exists: a
     * context holds exactly one scroll action, so a second subscriber would
     * displace the first. An overlay that wants the wheel without taking it
     * away from the application polls this instead -- the same arrangement
     * getCurrentPosition() already provides for cursor motion.
     *
     * Accumulated rather than latched, so several events arriving between two
     * frames are all seen: a fast flick is many notches, not one.
     */
    [[nodiscard]] WMAMouseScroll consumeScrollDelta() noexcept;
    void setCursorEnabled(bool enabled);
    [[nodiscard]] bool isCursorEnabled() const;
    void setSensitivity(f64 sensitivity);
    [[nodiscard]] f64 getSensitivity() const;

    void processPendingEvents(const PendingEvent &event);

  protected:
    virtual void updateCursorState() = 0;

    void dispatchButtonPress(i32 button);
    void dispatchButtonRelease(i32 button);
    void dispatchMove(const WMAMousePosition &position);
    void dispatchScroll(const WMAMouseScroll &scroll);

    void ensureContextCapacity(InputContextId context);

    InputContextStack contexts_;
    std::vector<InputBindingTable<MouseAction, MOUSE_BUTTON_COUNT>> buttonBindings_;
    std::vector<MouseAction> moveActions_;
    std::vector<MouseAction> scrollActions_;

    WMAMousePosition currentPosition_;
    WMAMousePosition lastPosition_;

    //! Drained by consumeScrollDelta(); see it for why this is accumulated
    //! rather than simply holding the most recent event.
    WMAMouseScroll accumulatedScroll_{};

    bool cursorEnabled_ = true;
    f64 sensitivity_ = 1.0;
    bool firstMouse_ = true;
};

} // namespace wma

#endif // WMA_INPUT_MOUSE_LISTENER_HPP
