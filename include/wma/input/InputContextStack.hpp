#ifndef WMA_INPUT_INPUT_CONTEXT_STACK_HPP
#define WMA_INPUT_INPUT_CONTEXT_STACK_HPP

#include <vector>

#include "wma/input/InputTypes.hpp"

namespace wma
{

/**
 * @brief Manages a set of named input contexts and resolves which one is
 * "active" at dispatch time.
 *
 * Supports two interaction models:
 *   - Switch mode: setActiveContext() changes the flat active context.
 *   - Stack mode:  pushContext() / popContext() layers temporary overlays
 *                  (e.g. a pause menu on top of gameplay). The top of the
 *                  stack always wins over the active context.
 * Both models can be mixed. Resolution is O(1): check stack.back() or fall
 * back to activeContext_.
 */
class InputContextStack
{
  public:
    constexpr InputContextStack() noexcept : activeContext_(INPUT_CONTEXT_DEFAULT), nextContextId_(1)
    {
    }

    //! Returns a new unique context ID. IDs are never reused.
    [[nodiscard]] InputContextId createContext() noexcept
    {
        return nextContextId_++;
    }

    //! Flat-switch: change the active context (used when stack is empty).
    void setActiveContext(InputContextId context) noexcept
    {
        activeContext_ = context;
    }

    //! Stack overlay: push a context on top; it wins until popped.
    void pushContext(InputContextId context)
    {
        stack_.push_back(context);
    }

    //! Remove the top stack overlay. Safe to call on an empty stack.
    void popContext() noexcept
    {
        if (!stack_.empty())
            stack_.pop_back();
    }

    //! The context that should be used for dispatch right now.
    [[nodiscard]] InputContextId resolved() const noexcept
    {
        return stack_.empty() ? activeContext_ : stack_.back();
    }

    [[nodiscard]] InputContextId activeContext() const noexcept
    {
        return activeContext_;
    }

    //! Total number of contexts that have been created (including default = 0).
    [[nodiscard]] InputContextId contextCount() const noexcept
    {
        return nextContextId_;
    }

  private:
    InputContextId activeContext_;
    InputContextId nextContextId_;
    std::vector<InputContextId> stack_;
};

} // namespace wma

#endif // WMA_INPUT_INPUT_CONTEXT_STACK_HPP
