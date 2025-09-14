#pragma once
#include <cstdint>
#include <cstddef>
#include <libsdb/types.hpp>

namespace sdb {
    class process;

    class watchpoint {
    public:
        using id_type = std::int32_t;

        watchpoint() = delete;
        watchpoint(const watchpoint&) = delete;
        watchpoint& operator=(const watchpoint&) = delete;

        void enable();

        void disable();

        [[nodiscard]] id_type id() const noexcept { return m_id; }

        [[nodiscard]] bool is_enabled() const noexcept { return m_is_enabled; }

        [[nodiscard]] virt_addr address() const noexcept { return m_address; }

        [[nodiscard]] stoppoint_mode mode() const noexcept { return m_mode; }

        [[nodiscard]] std::size_t size() const noexcept { return m_size; }

        [[nodiscard]] bool at_address(virt_addr addr) const noexcept {
            return m_address == addr;
        }

        [[nodiscard]] bool in_range(virt_addr low, virt_addr high) const noexcept {
            return low <= m_address and high > m_address;
        }

        [[nodiscard]] std::uint64_t data() const { return m_data; }
        [[nodiscard]] std::uint64_t previous_data() const { return m_previous_data; }

        void update_data();
    private:
        friend process;

        watchpoint(process& proc, virt_addr address, stoppoint_mode mode, std::size_t size);

        const id_type m_id;
        process* m_process;
        const virt_addr m_address;
        const stoppoint_mode m_mode;
        const std::size_t m_size;
        bool m_is_enabled;
        int m_hardware_register_index = -1;

        std::uint64_t m_data = 0;
        std::uint64_t m_previous_data = 0;
    };
}