#ifndef WMA_INPUT_INPUT_BINDING_TABLE_HPP
#define WMA_INPUT_INPUT_BINDING_TABLE_HPP

#include <array>
#include <cassert>

#include <ink/ink_base.hpp>

namespace wma
{

/**
 * @brief Fixed-size O(1) table mapping integer slot indices to Actions.
 *
 * Template parameter N must equal the total number of valid indices
 * (e.g. KEY_COUNT for keyboard, MOUSE_BUTTON_COUNT for mouse).
 * All slots default-initialise to Action{} (an empty/unbound action).
 */
template <typename Action, usize N> class InputBindingTable
{
  public:
    //! Single subscript operator using C++23 deducing-this: one definition
    //! handles both const and mutable references, returning the correct
    //! reference category without code duplication.
    template <typename Self> [[nodiscard]] decltype(auto) operator[](this Self &&self, usize index)
    {
        assert(index < N);
        return std::forward<Self>(self).slots_[index];
    }

    constexpr void clear() noexcept
    {
        slots_.fill(Action{});
    }

    constexpr void clearSlot(usize index) noexcept
    {
        assert(index < N);
        slots_[index] = Action{};
    }

    [[nodiscard]] constexpr bool has(usize index) const noexcept
    {
        assert(index < N);
        return slots_[index].isBound();
    }

    [[nodiscard]] static constexpr usize size() noexcept
    {
        return N;
    }

  private:
    std::array<Action, N> slots_{};
};

} // namespace wma

#endif // WMA_INPUT_INPUT_BINDING_TABLE_HPP
