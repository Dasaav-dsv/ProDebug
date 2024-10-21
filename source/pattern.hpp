#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace Index {
template <size_t N>
struct CPattern {
    static_assert(N >= 6, "must be at least 2 octets long");
    static_assert(N % 3 == 0, "must contain space-delimited hexadecimal octets and a \\0");
    static_assert(N / 3 <= std::numeric_limits<uint16_t>::max(), "must be at most UINT16_MAX long");

    consteval CPattern(const char(&str)[N]) {
        for (size_t i = 0; i < N; i += 3) {
            uint8_t hi4 = base36(str[i]);
            uint8_t lo4 = base36(str[i + 1]);
            if (hi4 < 16 && lo4 < 16) {
                bt[len] = (hi4 << 4) + lo4;
                pos[len] = static_cast<uint16_t>(i / 3);
                ++len;
            }
            // Every separator must be a space
            if (i + 3 < N && str[i + 2] != ' ')
                nonSpaceSeparator();
        }
        if (len < 2) // Shorter than two bytes
            tooShort();
    }

    // Quick base36 ASCII to numeric value lookup
    static constexpr uint8_t base36(char c) {
        // clang-format off
        const char* t = "\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24" \
                        "\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24" \
                        "\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24\x24" \
                        "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x24\x24\x24\x24\x24\x24" \
                        "\x24\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18" \
                        "\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x24\x24\x24\x24" \
                        "\x24\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18" \
                        "\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21\x22\x23\x24\x24\x24\x24\x24";
        // clang-format on
        return c < 0 ? 36 : t[static_cast<size_t>(c)];
    }

    // Octets are misaligned or not space delimited.
    // e.g. "01-??-23" must be "01 ?? 23"
    static void nonSpaceSeparator() {}

    // Pattern is too short to search for,
    // since it matches a single byte or no bytes
    static void tooShort() {}

    uint8_t bt[N / 3]{};
    uint16_t pos[N / 3]{};
    uint16_t len = 0;
};
} // namespace Index
