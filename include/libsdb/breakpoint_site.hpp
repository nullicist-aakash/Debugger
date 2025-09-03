#pragma once

#include <cstdint>
#include <cstddef>
#include <libsdb/types.hpp>

namespace sdb {
    class process;

    class breakpoint_site {
    public:
        using id_type = std::int32_t;

        breakpoint_site() = delete;
        breakpoint_site(const breakpoint_site&) = delete;
        breakpoint_site(breakpoint_site&&) = delete;

        /**
         * Enables the breakpoint. If it is already enabled, do nothing.
         */
        void enable();

        /**
         * Disables the breakpoint, if it is enabled. no-op otherwise.
         */
        void disable();

        [[nodiscard]] id_type id() const noexcept {
            return m_id;
        }

        [[nodiscard]] bool is_enabled() const  noexcept {
            return m_is_enabled;
        }

        [[nodiscard]] virt_addr address() const noexcept {
            return m_address;
        }

        [[nodiscard]] bool in_range(const virt_addr& low, const virt_addr& high) const noexcept {
            return low <= m_address && m_address < high;
        }

        [[nodiscard]] bool is_hardware() const noexcept { return m_is_hardware; }

        [[nodiscard]] bool is_internal() const noexcept { return m_is_internal; }

    private:
        breakpoint_site(process& proc, virt_addr addr, bool is_hardware = false, bool is_internal = false);
        friend class process;

        process* m_process;
        const id_type m_id;
        const virt_addr m_address;
        bool m_is_enabled;
        std::byte m_saved_data;   // The byte in assembly which was replaced by interrupt

        const bool m_is_hardware;
        const bool m_is_internal;
        int m_hardware_register_index = -1;
    };
}