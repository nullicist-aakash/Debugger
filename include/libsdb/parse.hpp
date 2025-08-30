#pragma once

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <array>
#include <cstddef>

namespace sdb {
    template <class I>
    std::optional<I> to_integral(std::string_view sv, int base = 10) {
        auto begin = sv.begin();
        if (base == 16 and sv.size() > 1 and begin[0] == '0' and begin[1] == 'x')
            begin += 2;

        I ret;
        if (auto result = std::from_chars(begin, sv.end(), ret, base); result.ptr != sv.end())
            return std::nullopt;
        return ret;
    }

    template<>
    inline std::optional<std::byte> to_integral(std::string_view sv, int base) {
        if (auto uint8 = to_integral<std::uint8_t>(sv, base))
            return static_cast<std::byte>(*uint8);

        return std::nullopt;
    }

    template <class F>
    std::optional<F> to_float(std::string_view sv) {
        F ret;
        if (auto result = std::from_chars(sv.begin(), sv.end(), ret); result.ptr != sv.end())
            return std::nullopt;
        return ret;
    }

    template <std::size_t N>
    auto parse_vector(std::string_view text) {
        auto invalid = [] { sdb::error::send("Invalid format"); };

        std::array<std::byte, N> bytes;
        const char* c = text.data();

        if (*c++ != '[') invalid();

        for (auto i = 0; i < N - 1; ++i) {
            bytes[i] = to_integral<std::byte>({ c, 4 }, 16).value();
            c += 4;
            if (*c++ != ',') invalid();
        }

        bytes[N - 1] = to_integral<std::byte>({ c, 4 }, 16).value();

        if (*c++ != ']') invalid();
        if (c != text.end()) invalid();

        return bytes;
    }

    inline auto parse_vector(std::string_view text) {
        auto invalid = [] { sdb::error::send("Invalid format"); };

        std::vector<std::byte> bytes;
        const char* c = text.data();

        if (*c++ != '[') invalid();

        while (*c != ']') {
            auto byte = sdb::to_integral<std::byte>({ c, 4 }, 16);
            bytes.push_back(byte.value());
            c += 4;

            if (*c == ',') ++c;
            else if (*c != ']') invalid();
        }

        if (++c != text.end()) invalid();

        return bytes;
    }
}