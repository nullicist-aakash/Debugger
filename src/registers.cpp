#include <libsdb/registers.hpp>
#include <libsdb/bit.hpp>
#include <libsdb/process.hpp>
#include <type_traits>
#include <algorithm>


namespace {
    /**
     * Converts the smaller value into a wide type so that we can write to a register of any type.
     * @tparam T The value to write.
     * @param info The information of the target register to write to.
     * @param t The value to write.
     * @return The 128-bit equivalent of the type to write.
     */
    template <class T>
    sdb::byte128 widen(const sdb::register_info& info, T t) {
        using namespace sdb;

        if constexpr (std::is_floating_point_v<T>) {
            if (info.format == register_format::double_float)
                return to_byte128(static_cast<double>(t));
            if (info.format == register_format::long_double)
                return to_byte128(static_cast<long double>(t));
        }
        else if constexpr (std::is_signed_v<T>) {
            if (info.format == register_format::uint) {
                switch (info.size) {
                    case 2: return to_byte128(static_cast<std::uint16_t>(t));
                    case 4: return to_byte128(static_cast<std::uint32_t>(t));
                    case 8: return to_byte128(static_cast<std::uint64_t>(t));
                    default: break;
                }
            }
        }

        return sdb::to_byte128(t);
    }
}

sdb::registers::value sdb::registers::read(const register_info &info) const {
    auto bytes = to_bytes(m_data) + info.offset;
    if (info.format == register_format::uint) {
        switch (info.size) {
            case 1: return from_bytes_to<std::uint8_t>(bytes);
            case 2: return from_bytes_to<std::uint16_t>(bytes);
            case 4: return from_bytes_to<std::uint32_t>(bytes);
            case 8: return from_bytes_to<std::uint64_t>(bytes);
            default: error::send("Unexpected register size");
        }
    }
    if (info.format == register_format::double_float)
        return from_bytes_to<double>(bytes);
    if (info.format == register_format::long_double)
        return from_bytes_to<long double>(bytes);
    if (info.format == register_format::vector && info.size == 8)
        return from_bytes_to<byte64>(bytes);

    return from_bytes_to<byte128>(bytes);
}

void sdb::registers::write(const register_info &info, const value &val) {
    auto bytes = to_bytes(m_data);

    std::visit([&](auto& v) {
        if (sizeof(v) <= info.size) {
            auto wide = widen(info, v);
            auto val_bytes = to_bytes(wide);
            std::copy(val_bytes, val_bytes + info.size, bytes + info.offset);
        } else {
            error::send("sdb::registers::write called with mismatched register and value sizes");
        }
    }, val);

    if (info.type == register_type::fpr) {
        m_proc->write_fprs(m_data.i387);
    } else {
        // Trick to use to write AH, BH, CH, DH registers, as the ptrace API needs an 8-byte aligned address
        auto aligned_offset =  info.offset & ~0b111;
        // I am not sure if bytes + aligned_offset can overflow?
        m_proc->write_user_struct(aligned_offset, from_bytes_to<std::uint64_t>(bytes + aligned_offset));
    }
}
