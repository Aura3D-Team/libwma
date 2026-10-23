#ifdef WMA_ENABLE_ALSA

#include "audio/backends/alsa/AlsaApi.hpp"

#include "platform/DynLib.hpp"

namespace wma
{

inline namespace alsa_dyn
{
// NOLINTNEXTLINE(bugprone-macro-parentheses): the argument names a declarator
#define WMA_SYM(name) decltype(&::name) name = nullptr;
#include "AlsaSymbols.inc"
#undef WMA_SYM
} // namespace alsa_dyn

namespace alsa
{

bool load() noexcept
{
    static const bool loaded = []
    {
        static constexpr const char *kSonames[] = {"libasound.so.2"};
        static const dynlib::Binding kBindings[] = {
#define WMA_SYM(name) {#name, reinterpret_cast<void **>(&alsa_dyn::name)},
#include "AlsaSymbols.inc"
#undef WMA_SYM
        };
        return dynlib::bind(dynlib::open(kSonames), kBindings);
    }();
    return loaded;
}

} // namespace alsa

} // namespace wma

#endif // WMA_ENABLE_ALSA
