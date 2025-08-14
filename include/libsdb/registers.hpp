#pragma once

#include <sys/user.h>
#include <variant>
#include <libsdb/register_info.hpp>
#include <libsdb/types.hpp>

namespace sdb {
    class process;

    class registers {
    public:
        registers() = delete;
        registers(const registers&) = delete;
        registers& operator=(const registers&) = delete;

        using value = std::variant<
            std::uint8_t, std::uint16_t, std::uint32_t, std::uint64_t,
            std::int8_t, std::int16_t, std::int32_t, std::int64_t,
            float, double, long double,
            byte64, byte128
            >;

        [[nodiscard]] value read(const register_info& info) const;
        void write(const register_info& info, const value& val);

        template <typename T>
        T read_by_id_as(register_id id) const {
            return std::get<T>(read(register_info_by_id(id)));
        }

        void write_by_id(register_id id, const value& v) {
            return write(register_info_by_id(id), v);
        }

    private:
        friend process;
        explicit registers(process& proc) : m_proc{&proc} {}

        user m_data{};
        process* m_proc;
    };
}