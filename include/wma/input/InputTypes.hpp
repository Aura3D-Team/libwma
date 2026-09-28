#ifndef WMA_INPUT_INPUT_TYPES_HPP
#define WMA_INPUT_INPUT_TYPES_HPP

#include <cstdint>

#include "wma/input/keyboard/Keys.h"
#include <ink/ink_base.hpp>

namespace wma
{

using InputContextId = u32;

constexpr InputContextId INPUT_CONTEXT_DEFAULT = 0;
constexpr InputContextId INPUT_CONTEXT_INVALID = UINT32_MAX;
constexpr usize MOUSE_BUTTON_COUNT = 8;

} // namespace wma

#endif // WMA_INPUT_INPUT_TYPES_HPP
