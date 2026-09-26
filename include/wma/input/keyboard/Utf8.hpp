#ifndef WMA_INPUT_UTF8_HPP
#define WMA_INPUT_UTF8_HPP

#include <concepts>
#include <span>
#include <string_view>
#include <type_traits>

#include <ink/ink_base.hpp>

#include "wma/input/keyboard/KeyTypes.hpp"

/**
 * @file Utf8.hpp
 * @brief UTF-8 <-> @ref Codepoint conversion.
 *
 * Hand-rolled on purpose. The standard library offers nothing usable here:
 * `std::codecvt_utf8` and `std::wstring_convert` were deprecated in C++17 and
 * removed in C++26, `<text_encoding>` (C++26) only *identifies* an encoding
 * and cannot transcode, and `std::mbrtoc32` decodes the *current C locale's*
 * multibyte encoding rather than UTF-8 -- which would make this layer's
 * behaviour depend on a global the application owns, is not `constexpr`, and
 * carries `mbstate_t` the callers have no use for. Standardised transcoding
 * views (P2728) are not in C++26. Pulling in ICU or simdutf to decode a few
 * bytes per keystroke is not a trade a windowing library should make, so the
 * ~40 lines below stay.
 */

namespace wma::utf8
{

/**
 * @brief Why @ref decodeOne rejected a sequence; @ref DecodeError::Ok means it did not.
 *
 * The success enumerator is `Ok` rather than the more natural `None` because
 * `<X11/X.h>` defines `None` as an object-like macro, and the X11 backend
 * includes it before this header. Renaming it back would break that
 * translation unit and nothing else, which makes it a trap worth naming here.
 */
enum class DecodeError : u8
{
    Ok = 0,
    Truncated,           //!< Sequence runs past the end of the buffer.
    InvalidLead,         //!< Continuation byte with no lead, or a 5/6-byte form.
    InvalidContinuation, //!< A byte inside the sequence is not 10xxxxxx.
    Overlong,            //!< Encoded longer than the value requires.
    Surrogate,           //!< U+D800..U+DFFF, which UTF-8 must never carry.
    OutOfRange           //!< Above U+10FFFF.
};

/// One step of @ref decodeOne: the scalar value, where to resume, and why not.
struct DecodeResult
{
    Codepoint codepoint = 0; //!< Valid only when @ref ok() is true.
    usize next = 0;          //!< Index to resume decoding at.
    DecodeError error = DecodeError::Ok;

    [[nodiscard]] constexpr bool ok() const noexcept
    {
        return error == DecodeError::Ok;
    }
};

/**
 * @brief Decodes the single sequence beginning at @p index.
 *
 * The primitive @ref decode is built from, exposed because callers need it on
 * its own: a text field advancing a caret over a UTF-8 buffer, or code
 * validating a string without materialising the scalar values.
 *
 * @p next always advances (except at the end of the buffer), so a loop driven
 * by this function terminates whatever the input. On a malformed sequence it
 * points at the *offending byte* rather than past the whole sequence, because
 * that byte may itself begin a valid character and skipping it would swallow
 * one. On a rejected-but-well-formed sequence (overlong, surrogate, out of
 * range) the whole sequence is consumed -- there is nothing to resynchronise
 * on inside it.
 *
 * @param text  Bytes to decode; need not be null-terminated, may contain NUL.
 * @param index Offset of the lead byte.
 * @return The scalar value, or the reason it could not be produced.
 */
[[nodiscard]] constexpr DecodeResult decodeOne(std::string_view text, usize index) noexcept
{
    const usize size = text.size();

    if (index >= size)
        return {.next = size, .error = DecodeError::Truncated};

    const auto lead = static_cast<u8>(text[index]);

    //! ASCII: the overwhelmingly common case, and a single predictable branch.
    if (lead < 0x80u) [[likely]]
        return {.codepoint = static_cast<Codepoint>(lead), .next = index + 1u};

    u32 accumulator = 0;
    usize extraBytes = 0;
    u32 lowerBound = 0; //! Smallest value legally encodable at this length.

    if ((lead & 0xE0u) == 0xC0u)
    {
        accumulator = lead & 0x1Fu;
        extraBytes = 1;
        lowerBound = 0x80u;
    }
    else if ((lead & 0xF0u) == 0xE0u)
    {
        accumulator = lead & 0x0Fu;
        extraBytes = 2;
        lowerBound = 0x800u;
    }
    else if ((lead & 0xF8u) == 0xF0u)
    {
        accumulator = lead & 0x07u;
        extraBytes = 3;
        lowerBound = 0x10000u;
    }
    else
    {
        return {.next = index + 1u, .error = DecodeError::InvalidLead};
    }

    /*
     * Bounds are resolved once, before the read loop, rather than per byte.
     * Clamping to what is actually there (instead of testing `index + n < size`
     * inside the loop) keeps the two failures in the right order: a sequence
     * interrupted by some other byte must be reported as such even when it also
     * runs off the end, since that byte may begin a valid character.
     */
    const usize remaining = size - index - 1u;
    const usize available = remaining < extraBytes ? remaining : extraBytes;

    for (usize n = 1; n <= available; ++n)
    {
        const auto continuation = static_cast<u8>(text[index + n]);
        if ((continuation & 0xC0u) != 0x80u)
            return {.next = index + 1u, .error = DecodeError::InvalidContinuation};

        accumulator = (accumulator << 6) | (continuation & 0x3Fu);
    }

    //! Cut off by the end of the buffer: nothing follows to resynchronise on.
    if (available < extraBytes)
        return {.next = size, .error = DecodeError::Truncated};

    const usize next = index + extraBytes + 1u;

    if (accumulator < lowerBound)
        return {.next = next, .error = DecodeError::Overlong};

    //! Unsigned wraparound folds the D800..DFFF range test into one compare.
    if (accumulator - 0xD800u < 0x800u)
        return {.next = next, .error = DecodeError::Surrogate};

    if (accumulator > 0x10FFFFu)
        return {.next = next, .error = DecodeError::OutOfRange};

    return {.codepoint = static_cast<Codepoint>(accumulator), .next = next};
}

/**
 * @brief Decodes @p text, invoking @p emit once per Unicode scalar value.
 *
 * Three of the four window backends hand text over as UTF-8 bytes (SDL's
 * SDL_EVENT_TEXT_INPUT, X11's Xutf8LookupString, xkb's
 * xkb_state_key_get_utf8), so this is the shared step between them and the
 * decoded @ref Codepoint the input layer dispatches.
 *
 * Malformed input is skipped rather than aborting the whole run: text arrives
 * from the platform's IME and a caller has no way to repair it, so dropping
 * the bad byte and carrying on is the only behaviour that keeps the
 * surrounding well-formed characters. Overlong encodings, surrogate halves and
 * out-of-range values are rejected for the same reason they always are -- they
 * must never reach a caller as if they were valid.
 *
 * @param text  Bytes to decode; need not be null-terminated, may contain NUL.
 * @param emit  Invoked as emit(Codepoint) for each scalar value decoded.
 */
template <std::invocable<Codepoint> Emit>
constexpr void decode(std::string_view text, Emit &&emit) noexcept(std::is_nothrow_invocable_v<Emit &, Codepoint>)
{
    usize index = 0;

    while (index < text.size())
    {
        const DecodeResult result = decodeOne(text, index);

        //! A truncated tail is the end of the buffer by definition.
        if (result.error == DecodeError::Truncated)
            return;

        index = result.next;

        if (result.ok())
            emit(result.codepoint);
    }
}

/// Bytes @p codepoint encodes to, or 0 if it is not a Unicode scalar value.
[[nodiscard]] constexpr usize encodedLength(Codepoint codepoint) noexcept
{
    if (codepoint < 0x80u)
        return 1;
    if (codepoint < 0x800u)
        return 2;
    if (codepoint - 0xD800u < 0x800u)
        return 0; //! Surrogate half.
    if (codepoint < 0x10000u)
        return 3;
    if (codepoint <= 0x10FFFFu)
        return 4;

    return 0;
}

/**
 * @brief Encodes @p codepoint into @p out as UTF-8.
 *
 * The counterpart to @ref decode, and the other half of what a text-editing
 * caller needs: the input layer hands it scalar values, and its own buffer is
 * UTF-8. Rejects exactly what @ref decodeOne rejects, so a value that survives
 * a round trip through both is a genuine scalar value.
 *
 * @param out Fixed four-byte destination; the maximum any scalar value needs.
 * @return Bytes written, or 0 if @p codepoint is a surrogate or out of range
 *         (in which case @p out is untouched).
 */
[[nodiscard]] constexpr usize encode(Codepoint codepoint, std::span<char, 4> out) noexcept
{
    const usize length = encodedLength(codepoint);

    switch (length)
    {
    case 1:
        out[0] = static_cast<char>(codepoint);
        break;
    case 2:
        out[0] = static_cast<char>(0xC0u | (codepoint >> 6));
        out[1] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
        break;
    case 3:
        out[0] = static_cast<char>(0xE0u | (codepoint >> 12));
        out[1] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
        break;
    case 4:
        out[0] = static_cast<char>(0xF0u | (codepoint >> 18));
        out[1] = static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
        out[3] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
        break;
    default:
        break;
    }

    return length;
}

} // namespace wma::utf8

#endif // WMA_INPUT_UTF8_HPP
