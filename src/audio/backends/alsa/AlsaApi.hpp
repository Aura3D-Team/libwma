#ifndef WMA_AUDIO_BACKENDS_ALSA_API_HPP
#define WMA_AUDIO_BACKENDS_ALSA_API_HPP

#include <alsa/asoundlib.h>

//! libasound is bound at run time. Inside namespace wma these pointers hide the C functions of the same name, so
//! call sites stay as they are; one outside the namespace fails to link instead of binding libasound at load time.

namespace wma
{

inline namespace alsa_dyn
{
// NOLINTNEXTLINE(bugprone-macro-parentheses): the argument names a declarator
#define WMA_SYM(name) extern decltype(&::name) name;
#include "AlsaSymbols.inc"
#undef WMA_SYM
} // namespace alsa_dyn

namespace alsa
{

//! Loads libasound once per process. False when it is missing; the audio chain then moves on to the next backend.
[[nodiscard]] bool load() noexcept;

} // namespace alsa

} // namespace wma

#endif // WMA_AUDIO_BACKENDS_ALSA_API_HPP
