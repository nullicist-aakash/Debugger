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

        void enable();
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

    private:
        breakpoint_site(const process& proc, virt_addr addr);
        friend class process;

        const process* m_process;
        const id_type m_id;
        const virt_addr m_address;
        bool m_is_enabled;
        std::byte m_saved_data;   // The byte in assembly which was replaced by interrupt
    };
}