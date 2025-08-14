#pragma once

#include <cstring>
#include <cstddef>

#include <libsdb/types.hpp>

namespace sdb {
    /**
     * Converts the bytes to the target type.
     * @tparam To The target type for the bytes.
     * @param bytes The bytes to convert from.
     * @return The type-casted view of the byte.
     */
    template <class To>
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
    template <class From>
    std::byte* to_bytes(From& from) {
        return reinterpret_cast<std::byte*>(&from);
    }

    /**
     * Converts a type to its byte equivalent.
     * @tparam From Type to convert.
     * @param from Instance of From type, which we want to view as bytes.
     * @return The byte representation of the type.
     */
    template <class From>
    const std::byte* to_bytes(const From& from) {
        return reinterpret_cast<const std::byte*>(&from);
    }

    /**
     * Converts a type to its byte64 equivalent. The type should be less than or equal to 8 bytes for this operation.
     * @tparam From Type to convert.
     * @param src Instance of From type, which we want to view as byte64.
     * @return The byte representation of the type.
     */
    template <class From>
    byte64 to_byte64(From src) {
        static_assert(sizeof(From) <= sizeof(byte64), "The original type should be less than or equal to 8 bytes before converting them to byte64");
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
    template <class From>
    byte128 to_byte128(From src) {
        static_assert(sizeof(From) <= sizeof(byte128), "The original type should be less than or equal to 16 bytes before converting them to byte128");
        byte128 ret{};
        std::memcpy(&ret, &src, sizeof(From));
        return ret;
    }
}
