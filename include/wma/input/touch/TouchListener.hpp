#ifndef WMA_INPUT_TOUCH_LISTENER_HPP
#define WMA_INPUT_TOUCH_LISTENER_HPP

#include <vector>

#include "wma/input/InputContextStack.hpp"
#include "wma/input/InputTypes.hpp"
#include "wma/input/touch/TouchInputCallback.hpp"
#include "wma/input/touch/TouchTypes.hpp"

namespace wma
{

/**
 * @class TouchListener
 * @brief Per-finger touch input, for gestures a mouse cannot express.
 *
 * SDL can synthesize mouse events from touch, and for simple "tap = click"
 * cases that is enough. But a mouse has a single cursor, so synthesized events
 * collapse every finger into one stream -- a twin-stick layout (walk with the
 * left thumb *while* looking with the right) is impossible to express that
 * way. This listener reports each finger separately, keyed by a stable
 * @ref TouchFingerId, so callers can track any number of them concurrently.
 *
 * Coordinates are window pixels and deltas are Y-up, matching
 * @ref WMAMousePosition, so touch and mouse handling code can share the same
 * math rather than each caller re-normalizing SDL's 0..1 touch range.
 *
 * Contexts work exactly like MouseListener's/KeyboardListener's: bindings live
 * per @ref InputContextId, so a menu can shadow gameplay bindings and pop back.
 */
class TouchListener
{
  public:
    TouchListener();
    virtual ~TouchListener();

    TouchListener(const TouchListener &) = delete;
    TouchListener &operator=(const TouchListener &) = delete;
    TouchListener(TouchListener &&) = default;
    TouchListener &operator=(TouchListener &&) = default;

    //! Context management.
    [[nodiscard]] InputContextId createContext();
    void setActiveContext(InputContextId context);
    void pushContext(InputContextId context);
    void popContext();
    [[nodiscard]] InputContextId getActiveContext() const;
    [[nodiscard]] InputContextId getResolvedContext() const;

    //! A finger touched down.
    void setDownAction(TouchInputCallback action);
    void setDownAction(TouchInputCallback action, InputContextId context);
    //! A finger moved while down.
    void setMoveAction(TouchInputCallback action);
    void setMoveAction(TouchInputCallback action, InputContextId context);
    //! A finger lifted.
    void setUpAction(TouchInputCallback action);
    void setUpAction(TouchInputCallback action, InputContextId context);

    void clearAllActions();
    void clearAllActions(InputContextId context);

  protected:
    void dispatchDown(const WMATouchPoint &point);
    void dispatchMove(const WMATouchPoint &point);
    void dispatchUp(const WMATouchPoint &point);

    void ensureContextCapacity(InputContextId context);

    InputContextStack contexts_;
    std::vector<TouchInputCallback> downActions_;
    std::vector<TouchInputCallback> moveActions_;
    std::vector<TouchInputCallback> upActions_;
};

} // namespace wma

#endif // WMA_INPUT_TOUCH_LISTENER_HPP
