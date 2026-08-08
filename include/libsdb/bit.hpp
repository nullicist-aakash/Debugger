#pragma once

#include <cstring>
#include <cstddef>
#include <vector>
#include <ranges>
#include <span>
#include <bit>
#include <string_view>

#include <libsdb/types.hpp>

namespace sdb {
    inline std::string_view to_string_view(std::span<const std::byte> data) {
        return { reinterpret_cast<const char*>(data.data()), data.size() };
    }

    /**
     * Converts the bytes to the target type.
     * @tparam To The target type for the bytes.
     * @param bytes The bytes to convert from.
     * @return The type-casted view of the byte.
     */
    template <typename To> requires std::is_trivially_copyable_v<To>
    To from_bytes_to(const std::byte* bytes) {
        To ret;
        std::memcpy(&ret, bytes, sizeof(To));
        return ret;
    }

    /**
     * Converts a type to its byte equivalent.
     * @tparam From Type to convert.
     * @param from Instance of From type, which we want to view as bytes.
     * @return The byte representation of the type.
     */
    template <class From> requires std::is_trivially_copyable_v<From>
    std::span<std::byte, sizeof(From)> to_bytes(From& from) {
        return std::span<std::byte, sizeof(From)>(reinterpret_cast<std::byte*>(&from), sizeof(From));
    }

    /**
     * Converts a type to its byte equivalent.
     * @tparam From Type to convert.
     * @param from Instance of From type, which we want to view as bytes.
     * @return The byte representation of the type.
     */
    template <class From> requires std::is_trivially_copyable_v<From>
    std::span<const std::byte, sizeof(From)> to_bytes(const From& from) {
        return std::span<const std::byte, sizeof(From)>(reinterpret_cast<const std::byte*>(&from), sizeof(From));
    }

    /**
     * Converts a type to its byte64 equivalent. The type should be less than or equal to 8 bytes for this operation.
     * @tparam From Type to convert.
     * @param src Instance of From type, which we want to view as byte64.
     * @return The byte representation of the type.
     */
    template <class From> requires std::is_trivially_copyable_v<From> && (sizeof(From) <= sizeof(byte64))
    byte64 to_byte64(From src) {
        byte64 ret{};
        std::memcpy(&ret, &src, sizeof(From));
        return ret;
    }

    /**
     * Converts a type to its byte128 equivalent. The type should be less than or equal to 16 bytes for this operation.
     * @tparam From Type to convert.
     * @param src Instance of From type, which we want to view as byte128.
     * @return The byte representation of the type.
     */
    template <class From> requires std::is_trivially_copyable_v<From> && (sizeof(From) <= sizeof(byte128))
    byte128 to_byte128(From src) {
        byte128 ret{};
        std::memcpy(&ret, &src, sizeof(From));
        return ret;
    }
}
