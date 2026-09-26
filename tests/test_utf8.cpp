/**
 * @file test_utf8.cpp
 * @brief Covers wma::utf8, the shared step between the UTF-8 three of the four
 *        window backends hand over and the decoded Codepoint the input layer
 *        dispatches.
 *
 * Malformed input is the substance of this: the bytes come from the platform's
 * IME, so a caller cannot repair them, and the decoder's contract is to drop
 * exactly the bad byte while preserving every well-formed character around it.
 */
#include "wma/input/keyboard/Utf8.hpp"

#include <array>
#include <cstdio>
#include <string_view>
#include <vector>

namespace
{

int g_failures = 0;

[[nodiscard]] std::vector<wma::Codepoint> decodeAll(std::string_view text)
{
    std::vector<wma::Codepoint> out;
    wma::utf8::decode(text,
                      [&out](wma::Codepoint c)
                      {
                          out.push_back(c);
                      });
    return out;
}

void check(const char *name, std::string_view input, const std::vector<wma::Codepoint> &expected)
{
    const std::vector<wma::Codepoint> actual = decodeAll(input);

    if (actual == expected)
    {
        std::printf("ok   %s\n", name);
        return;
    }

    std::printf("FAIL %-32s got[", name);
    for (const wma::Codepoint c : actual)
        std::printf("%X ", c);
    std::printf("] want[");
    for (const wma::Codepoint c : expected)
        std::printf("%X ", c);
    std::printf("]\n");
    ++g_failures;
}

void checkStep(const char *name, std::string_view input, wma::utf8::DecodeError expectedError, usize expectedNext)
{
    const wma::utf8::DecodeResult r = wma::utf8::decodeOne(input, 0);

    if (r.error == expectedError && r.next == expectedNext)
    {
        std::printf("ok   %s\n", name);
        return;
    }

    std::printf("FAIL %-32s got[err=%d next=%zu] want[err=%d next=%zu]\n", name, static_cast<int>(r.error),
                static_cast<std::size_t>(r.next), static_cast<int>(expectedError),
                static_cast<std::size_t>(expectedNext));
    ++g_failures;
}

//! Encodes @p codepoint and decodes it back, asserting both halves agree.
void checkRoundTrip(const char *name, wma::Codepoint codepoint, usize expectedLength)
{
    std::array<char, 4> buffer{};
    const usize length = wma::utf8::encode(codepoint, buffer);

    const bool sizeOk = (length == expectedLength);
    const bool valueOk =
        (length == 0) || (decodeAll(std::string_view{buffer.data(), length}) == std::vector<wma::Codepoint>{codepoint});

    if (sizeOk && valueOk)
    {
        std::printf("ok   %s\n", name);
        return;
    }

    std::printf("FAIL %-32s U+%04X encoded to %zu byte(s), wanted %zu%s\n", name, codepoint,
                static_cast<std::size_t>(length), static_cast<std::size_t>(expectedLength),
                valueOk ? "" : " (round trip mismatch)");
    ++g_failures;
}

/*
 * The decoder is constexpr so a caller can validate a compile-time string, and
 * the property is easy to lose to a stray non-constexpr call. Asserting it here
 * turns that regression into a build failure.
 */
constexpr wma::Codepoint firstOf(std::string_view text)
{
    wma::Codepoint first = 0;
    bool seen = false;
    wma::utf8::decode(text,
                      [&](wma::Codepoint c)
                      {
                          if (!seen)
                          {
                              first = c;
                              seen = true;
                          }
                      });
    return first;
}

static_assert(firstOf("\xF0\x9F\x8E\xAE") == 0x1F3AE, "decode must be usable at compile time");
static_assert(wma::utf8::encodedLength(0x1F3AE) == 4);
static_assert(wma::utf8::encodedLength(0xD800) == 0, "surrogates are not encodable");

} // namespace

int main()
{
    //! Well-formed input, one case per encoded length.
    check("ascii", "Hi!", {'H', 'i', '!'});
    check("2-byte e-acute", "\xC3\xA9", {0xE9});
    check("3-byte CJK", "\xE6\x97\xA5", {0x65E5});
    check("4-byte emoji", "\xF0\x9F\x8E\xAE", {0x1F3AE});
    check("mixed widths",
          "a\xC3\xA9\xF0\x9F\x8E\xAE"
          "z",
          {'a', 0xE9, 0x1F3AE, 'z'});
    check("BOM",
          "\xEF\xBB\xBF"
          "a",
          {0xFEFF, 'a'});

    //! A string_view may carry an embedded NUL; U+0000 is a scalar value.
    check("embedded NUL", std::string_view{"a\0b", 3}, {'a', 0, 'b'});

    /*
     * Range boundaries, where an off-by-one in the surrogate or maximum test
     * would show up and nowhere else.
     */
    check("U+007F", "\x7F", {0x7F});
    check("U+0080", "\xC2\x80", {0x80});
    check("U+07FF", "\xDF\xBF", {0x7FF});
    check("U+0800", "\xE0\xA0\x80", {0x800});
    check("U+D7FF below surr", "\xED\x9F\xBF", {0xD7FF});
    check("U+E000 above surr", "\xEE\x80\x80", {0xE000});
    check("U+FFFF", "\xEF\xBF\xBF", {0xFFFF});
    check("U+10000", "\xF0\x90\x80\x80", {0x10000});
    check("U+10FFFF max", "\xF4\x8F\xBF\xBF", {0x10FFFF});
    check("U+110000 rejected", "\xF4\x90\x80\x80", {});

    //! Values that are structurally decodable but must never be emitted.
    check("overlong slash", "\xC0\xAF", {});
    check("overlong 3-byte", "\xE0\x80\xAF", {});
    check("overlong 4-byte", "\xF0\x80\x80\xAF", {});
    check("overlong NUL", "\xC0\x80", {});
    check("surrogate half", "\xED\xA0\x80", {});
    check("surrogate high end", "\xED\xBF\xBF", {});
    check("above U+10FFFF", "\xF7\xBF\xBF\xBF", {});
    check("F5 lead", "\xF5\x80\x80\x80", {});
    check("5-byte form", "\xF8\x88\x80\x80\x80", {});
    check("FE byte", "\xFE", {});

    /*
     * Resynchronisation. The distinction these pin down is the one the decoder
     * gets wrong most easily: a sequence interrupted by another byte must
     * re-examine that byte (it may begin a valid character), while a sequence
     * cut off by the end of the buffer has nothing to resync on. A rejected but
     * *well-formed* sequence is consumed whole -- there is no lead byte hiding
     * inside it.
     */
    check("lone continuation",
          "\x80"
          "A",
          {'A'});
    check("bad byte mid-seq",
          "\xE6"
          "A",
          {'A'});
    check("truncated tail", "A\xE6\x97", {'A'});
    check("text after overlong",
          "\xC0\xAF"
          "A",
          {'A'});
    check("text after surrogate",
          "\xED\xA0\x80"
          "A",
          {'A'});
    check("lead interrupts lead", "\xE6\xC3\xA9", {0xE9});
    check("run of bad bytes",
          "\x80\x80\x80"
          "A",
          {'A'});
    check("truncated mid-buffer only at end", "\xF0\x9F\x8E", {});

    check("empty", "", {});

    //! decodeOne reports *why*, and always advances, so no caller can spin.
    checkStep("step: invalid lead", "\x80", wma::utf8::DecodeError::InvalidLead, 1);
    checkStep("step: bad continuation",
              "\xE6"
              "A",
              wma::utf8::DecodeError::InvalidContinuation, 1);
    checkStep("step: truncated", "\xE6\x97", wma::utf8::DecodeError::Truncated, 2);
    checkStep("step: overlong", "\xC0\xAF", wma::utf8::DecodeError::Overlong, 2);
    checkStep("step: surrogate", "\xED\xA0\x80", wma::utf8::DecodeError::Surrogate, 3);
    checkStep("step: out of range", "\xF7\xBF\xBF\xBF", wma::utf8::DecodeError::OutOfRange, 4);
    checkStep("step: past end", "", wma::utf8::DecodeError::Truncated, 0);
    checkStep("step: ascii ok", "A", wma::utf8::DecodeError::Ok, 1);

    //! encode is the inverse of decode over exactly the scalar values.
    checkRoundTrip("encode ascii", 'A', 1);
    checkRoundTrip("encode U+0080", 0x80, 2);
    checkRoundTrip("encode U+07FF", 0x7FF, 2);
    checkRoundTrip("encode U+0800", 0x800, 3);
    checkRoundTrip("encode U+FFFF", 0xFFFF, 3);
    checkRoundTrip("encode U+10000", 0x10000, 4);
    checkRoundTrip("encode U+10FFFF", 0x10FFFF, 4);
    checkRoundTrip("encode surrogate", 0xD800, 0);
    checkRoundTrip("encode too large", 0x110000, 0);

    if (g_failures > 0)
    {
        std::printf("\n%d failure(s)\n", g_failures);
        return 1;
    }

    std::printf("\nall passed\n");
    return 0;
}
